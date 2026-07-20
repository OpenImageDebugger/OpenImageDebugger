"""The oid-mcp stdio server: MCP tools over a live OID debug session."""

from __future__ import annotations

import importlib.util
import json
import os
import sys

# The published MCP SDK exposes the server class as FastMCP; some builds
# expose it as MCPServer under mcp.server.mcpserver. Choose by whether that
# module actually exists (find_spec) rather than by catching ImportError from
# the import itself -- so a genuine failure *inside* an existing mcpserver
# module surfaces to the user instead of being silently swallowed into the
# FastMCP fallback.
try:
    _has_mcpserver = (
        importlib.util.find_spec('mcp.server.mcpserver') is not None)
except (ImportError, ValueError):
    _has_mcpserver = False
if _has_mcpserver:
    from mcp.server.mcpserver import MCPServer, Image
else:
    from mcp.server.fastmcp import FastMCP as MCPServer, Image

from .analysis import compute_stats, dump_npy, extract_values
from .buffers import BufferCache, decode_buffer
from .discovery import (NoSessionError, _pid_alive, _reap, live_sessions,
                        live_viewers, pick_session)
from .protocol import DEFAULT_MAX_BYTES, ControlClient, ControlError
from .render import render_view
from .viewer_meta import to_bridge_meta

_HINTS = {
    'symbol_not_found': 'Call list_buffers() to see what is observable '
                        'at the current stop.',
    'unknown_buffer': 'Call list_buffers() to see what buffers this '
                      'viewer has.',
    'not_stopped': 'The debuggee is running; wait for the next '
                   'breakpoint or pause it, then retry.',
    'no_viewer': 'No OID viewer window is connected to this session.',
    'too_large': 'Buffer exceeds the transfer cap '
                 '(OID_MCP_MAX_BYTES, default 256 MiB).',
    'timeout': 'The debugger did not answer in time; it may be busy.',
}


def _friendly(error: Exception) -> str:
    if isinstance(error, ControlError):
        hint = _HINTS.get(error.code, '')
        return f'{error.message}' + (f' {hint}' if hint else '')
    return str(error)


def _parse_max_bytes() -> int:
    """
    Read the OID_MCP_MAX_BYTES transfer cap defensively. A missing, invalid,
    or non-positive value falls back to the default (with a stderr note) so a
    bad env var can never crash the server at import time.
    """
    raw = os.environ.get('OID_MCP_MAX_BYTES')
    if raw is None:
        return DEFAULT_MAX_BYTES
    try:
        value = int(raw)
    except ValueError:
        value = 0
    if value <= 0:
        sys.stderr.write(
            '[oid-mcp] Ignoring invalid OID_MCP_MAX_BYTES=%r (expected a '
            'positive integer number of bytes); using default %d.\n'
            % (raw, DEFAULT_MAX_BYTES))
        return DEFAULT_MAX_BYTES
    return value


class SessionManager:
    """Per-stop buffer cache over live sessions (a fresh connection per call)."""

    def __init__(self):
        self._cache = BufferCache(capacity=4)
        self._max_bytes = _parse_max_bytes()

    def _connect(self, info):
        return ControlClient('127.0.0.1', info.port, info.token)

    def _call_debugger(self, session, fn):
        client = self._connect(self._resolve(session))
        try:
            return fn(client)
        finally:
            client.close()

    def _call_viewer(self, session, fn):
        # A viewer discovery file can outlive its endpoint while the pid
        # that wrote it lives on (a long-lived embedding host whose bridge
        # is gone): the reader's pid probe passes but the port refuses.
        # Treat connection-refused as "dead viewer": reap that file and
        # re-resolve so another live viewer can win. Bounded, so a
        # pathological discovery dir cannot loop forever.
        for _ in range(8):
            info = self._resolve_viewer(session)
            try:
                client = self._connect(info)
            except ConnectionRefusedError:
                if info.path is not None:
                    _reap(info.path)
                continue
            try:
                return fn(client)
            finally:
                client.close()
        raise NoSessionError(
            'viewer discovery entries kept refusing connections; restart '
            'the viewer window and retry.')

    def _resolve(self, session):
        return pick_session(live_sessions(), session)

    def _resolve_viewer(self, session):
        """
        Select a live viewer window by viewer pid or paired debugger pid.

        No selector: the only live viewer if there is exactly one, else
        the most recently started; none live raises a friendly
        `NoSessionError` pointing at `OID_AGENT=1` (paired viewer) or a
        standalone `oidwindow`.

        An explicit `session` is tried first against live viewers' own
        pid; if none matches, it is tried against their `debugger_pid`
        pairing instead. A debugger with no paired viewer window (e.g.
        before its first stop) raises a distinct "that debugger has no
        viewer window" error rather than silently falling back to an
        unrelated window. Should the pairing ever resolve to more than
        one live viewer (a transient window relaunch racing discovery
        reaping), liveness is re-checked and the most recently started
        one wins.
        """
        viewers = live_viewers()
        if session is None:
            if not viewers:
                raise NoSessionError(
                    'no live OID viewer window found. Start a debugger '
                    'with OID_AGENT=1 in its environment (a viewer '
                    'window is spawned at its first stop), or run the '
                    'standalone `oidwindow` with OID_AGENT=1 set in its '
                    'own environment.')
            return max(viewers, key=lambda v: v.start_time)

        for viewer in viewers:
            if viewer.pid == session:
                return viewer

        paired = [v for v in viewers if v.debugger_pid == session]
        if not paired:
            raise NoSessionError(
                f'no live OID viewer for pid {session}: it is not a '
                f'live viewer itself, and that debugger has no viewer '
                f'window paired to it.')
        if len(paired) > 1:
            alive = [v for v in paired if _pid_alive(v.pid)]
            paired = alive or paired
        return max(paired, key=lambda v: v.start_time)

    def _resolve_pixel_source(self, session):
        """
        Resolve a pixel-tool `session` selector to its source.

        Prefers a debugger session, as before: `_resolve` picks the
        explicit match, or (with no selector) the most recently started
        live debugger session. Only when that fails -- the pid selects
        no live debugger session, or none exist at all -- is `session`
        instead resolved against live viewer windows (by the viewer's
        own pid, or by `debugger_pid` pairing), so a caller can target a
        standalone `oidwindow` or a viewer whose debugger session has
        already exited.
        """
        try:
            return 'debugger', self._resolve(session)
        except NoSessionError:
            return 'viewer', self._resolve_viewer(session)

    def list_buffers(self, session) -> dict:
        kind, _ = self._resolve_pixel_source(session)
        if kind == 'viewer':
            buffers = self._call_viewer(
                session, lambda c: c.list_viewer_buffers())
            return {'symbols': [b['name'] for b in buffers],
                    'stop_generation': 0}
        symbols, generation = self._call_debugger(
            session, lambda c: c.list_symbols())
        return {'symbols': symbols, 'stop_generation': generation}

    def fetch(self, session, symbol):
        """
        Return (meta, array).

        A debugger fetch is served from the per-stop cache. A viewer
        fetch is never cached: `viewer_meta.to_bridge_meta` always sets
        `stop_generation` to 0 because the viewer has no notion of a
        debugger stop -- a fetch from it is always the buffer's current
        state, so caching it under a constant key would serve stale
        pixels forever.
        """
        kind, _ = self._resolve_pixel_source(session)
        if kind == 'viewer':
            viewer_meta, raw = self._call_viewer(
                session, lambda c: c.get_buffer(symbol,
                                                max_bytes=self._max_bytes))
            meta = to_bridge_meta(viewer_meta)
            return meta, decode_buffer(meta, raw)

        # ping is functional here (not a liveness probe): it returns the
        # current stop generation, which keys the per-stop cache. One
        # connection serves ping + cache-miss get_buffer, so the key always
        # matches the endpoint that produced the bytes. info.token is
        # included because it is per-instance (a restarted endpoint
        # publishes a fresh random token): pid alone can be reused by an
        # unrelated later session that happens to reach the same stop
        # generation, and without the token that session would read back
        # the previous session's cached bytes.
        info = self._resolve(session)
        client = self._connect(info)
        try:
            generation = client.ping()
            key = (info.pid, info.token, symbol, generation)
            cached = self._cache.get(key)
            if cached is not None:
                return cached
            meta, raw = client.get_buffer(symbol, max_bytes=self._max_bytes)
        finally:
            client.close()
        arr = decode_buffer(meta, raw)
        self._cache.put(key, (meta, arr))
        return meta, arr

    def plot(self, session, symbol) -> None:
        kind, _ = self._resolve_pixel_source(session)
        if kind == 'viewer':
            raise RuntimeError(
                'the buffer is already displayed in that viewer')
        self._call_debugger(session, lambda c: c.plot(symbol))

    def set_view(self, session, **fields) -> dict:
        return self._call_viewer(session, lambda c: c.set_view(**fields))

    def get_view(self, session) -> dict:
        return self._call_viewer(session, lambda c: c.get_view())


_INSTRUCTIONS = (
    'EXPERIMENTAL. This server exposes image buffers from a live gdb/lldb '
    'debug session — i.e. debuggee memory. Treat what you read as '
    'sensitive, local-only data.\n'
    'Token usage: `view` returns a rendered PNG and buffers can be large, '
    'so responses may be token-heavy. Keep output small: bound `view` with '
    '`region`, `channel`, and `max_px`; use `stats` for summaries and '
    '`values` for a small exact crop (capped at 1024 numbers) rather than '
    'reading whole buffers; use `dump` to write a lossless .npy to disk '
    'instead of returning bulk data through the conversation.\n'
    'Viewers: `session` accepts either a debugger pid or an OID viewer '
    'window pid -- `list_sessions` lists both and pairs them via '
    '`debugger_pid`. `view`/`stats`/`values`/`dump`/`list_buffers` prefer '
    'a debugger session but fall back to a viewer window when none is '
    'live or one is selected explicitly, so they also work against a '
    'standalone `oidwindow`. `set_view`/`get_view` read or move what the '
    'human currently sees in their viewer window; `auto_contrast` is a '
    'global viewer setting, not per-buffer.'
)

# Security + token-usage warning for the human/logs. Goes to STDERR: on the
# stdio transport, stdout carries the MCP protocol and must not be written to.
_STARTUP_WARNING = (
    '[oid-mcp] EXPERIMENTAL feature.\n'
    '[oid-mcp] Security: this exposes debuggee memory from a live debug '
    'session to the connected agent. Enable only in trusted, local '
    'development (opt-in via OID_AGENT=1 on the debugger side).\n'
    '[oid-mcp] Token usage: image and large-buffer responses can consume '
    'significant tokens; bound `view` with region/channel/max_px and prefer '
    '`stats`/`values` over reading whole buffers.\n'
)

mcp = MCPServer('oid', instructions=_INSTRUCTIONS)
_manager = SessionManager()


def _fetch_or_fail(manager, session, symbol):
    try:
        return manager.fetch(session, symbol)
    except (ControlError, NoSessionError, ValueError) as error:
        raise RuntimeError(_friendly(error)) from None


def _format_info(info: dict) -> str:
    return '\n'.join(f'{key}: {json.dumps(value)}'
                     for key, value in info.items() if value is not None)


def _view_impl(manager, symbol, region, channel, vmin, vmax, max_px,
               session):
    meta, arr = _fetch_or_fail(manager, session, symbol)
    try:
        png, info = render_view(arr, meta, region=region, channel=channel,
                                vmin=vmin, vmax=vmax, max_px=max_px)
    except ValueError as error:
        raise RuntimeError(str(error)) from None
    return [Image(data=png, format='png'), _format_info(info)]


def _plot_impl(manager, symbol, session):
    try:
        manager.plot(session, symbol)
    except (ControlError, NoSessionError) as error:
        raise RuntimeError(_friendly(error)) from None
    return f'{symbol} plotted in the viewer'


@mcp.tool()
def list_sessions() -> dict:
    """List live OID debug sessions and viewer windows this server can reach.

    Each viewer shows its debugger_pid pairing (None for a standalone
    oidwindow), so a caller can target either a debugger's session pid
    or its viewer window explicitly.
    """
    return {
        'debuggers': [{'pid': s.pid, 'debugger': s.debugger,
                      'started_unix': s.start_time}
                     for s in live_sessions()],
        'viewers': [{'pid': v.pid, 'debugger_pid': v.debugger_pid,
                    'started_unix': v.start_time}
                   for v in live_viewers()],
    }


@mcp.tool()
def list_buffers(session: int | None = None) -> dict:
    """List symbols observable as image buffers at the current stop.

    session: a debugger or viewer pid from list_sessions(); defaults to
    the most recently started live debugger session, or the most
    recently started live viewer window if none is live.
    """
    try:
        return _manager.list_buffers(session)
    except (ControlError, NoSessionError) as error:
        raise RuntimeError(_friendly(error)) from None


@mcp.tool()
def view(symbol: str, region: list[int] | None = None,
         channel: int | None = None, vmin: float | None = None,
         vmax: float | None = None, max_px: int = 512,
         session: int | None = None) -> list:
    """Look at a buffer as an image.

    region: [x, y, w, h] crop in displayed coordinates (the stateless
    zoom: view the whole buffer, spot the artifact, view that region).
    channel: render one storage channel as grayscale.
    vmin/vmax: fix the normalization range (comparable across buffers);
    default normalizes per channel over the full buffer, so zooming
    never changes brightness. NaN pixels render magenta.
    """
    return _view_impl(_manager, symbol, region, channel, vmin, vmax,
                      max_px, session)


@mcp.tool()
def stats(symbol: str, region: list[int] | None = None,
          session: int | None = None) -> dict:
    """Per-channel min/max/mean/std and NaN/Inf/zero counts."""
    meta, arr = _fetch_or_fail(_manager, session, symbol)
    try:
        return compute_stats(arr, meta,
                             region=tuple(region) if region else None)
    except ValueError as error:
        raise RuntimeError(str(error)) from None


@mcp.tool()
def values(symbol: str, x: int, y: int, w: int, h: int,
           channel: int | None = None,
           session: int | None = None) -> dict:
    """Exact pixel values for a small crop (at most 1024 numbers)."""
    _, arr = _fetch_or_fail(_manager, session, symbol)
    try:
        return extract_values(arr, x, y, w, h, channel=channel)
    except ValueError as error:
        raise RuntimeError(str(error)) from None


@mcp.tool()
def dump(symbol: str, path: str | None = None, overwrite: bool = False,
         session: int | None = None) -> str:
    """Save the buffer losslessly as .npy; returns the file path.

    Refuses to overwrite an existing file unless overwrite=true, so an
    unintended path cannot clobber your data.
    """
    meta, arr = _fetch_or_fail(_manager, session, symbol)
    try:
        return dump_npy(arr, symbol, meta.get('stop_generation', 0),
                        path=path, overwrite=overwrite)
    except (OSError, ValueError) as error:
        raise RuntimeError(str(error)) from None


@mcp.tool()
def plot(symbol: str, session: int | None = None) -> str:
    """Mirror a buffer into the human's OID viewer window."""
    return _plot_impl(_manager, symbol, session)


@mcp.tool()
def set_view(session: int | None = None, buffer: str | None = None,
            center: list[float] | None = None, zoom: float | None = None,
            rotation_deg: float | None = None,
            channel: int | str | None = None,
            auto_contrast: bool | None = None) -> dict:
    """Set the OID viewer's view of a buffer (absolute/idempotent).

    Only the fields you pass are changed; omitted fields keep their
    current value. session: a viewer pid or a paired debugger pid, as
    for the pixel tools. channel selects a single display channel by
    0-based index (``0``, ``1``, ``2``) or ``"all"`` to restore the
    natural layout. auto_contrast is a global viewer setting (not
    per-buffer), matching get_view.
    """
    fields = {k: v for k, v in {
        'buffer': buffer, 'center': center, 'zoom': zoom,
        'rotation_deg': rotation_deg, 'channel': channel,
        'auto_contrast': auto_contrast}.items() if v is not None}
    try:
        return _manager.set_view(session, **fields)
    except (ControlError, NoSessionError) as error:
        raise RuntimeError(_friendly(error)) from None


@mcp.tool()
def get_view(session: int | None = None) -> dict:
    """Read the OID viewer's current view state.

    Returns buffer/center/zoom/rotation_deg/channel/auto_contrast.
    auto_contrast is global (not per-buffer).
    """
    try:
        return _manager.get_view(session)
    except (ControlError, NoSessionError) as error:
        raise RuntimeError(_friendly(error)) from None


def main() -> None:
    sys.stderr.write(_STARTUP_WARNING)
    sys.stderr.flush()
    mcp.run()


if __name__ == '__main__':
    main()
