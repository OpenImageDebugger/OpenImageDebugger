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
from .discovery import NoSessionError, live_sessions, pick_session
from .protocol import DEFAULT_MAX_BYTES, ControlClient, ControlError
from .render import render_view

_HINTS = {
    'symbol_not_found': 'Call list_buffers() to see what is observable '
                        'at the current stop.',
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
    """Connection pool + per-stop buffer cache over live sessions."""

    def __init__(self):
        self._clients: dict[int, ControlClient] = {}
        self._cache = BufferCache(capacity=4)
        self._max_bytes = _parse_max_bytes()

    def _client(self, info) -> ControlClient:
        client = self._clients.get(info.pid)
        if client is not None:
            try:
                client.ping()
                return client
            except OSError:
                client.close()
                del self._clients[info.pid]
        client = ControlClient('127.0.0.1', info.port, info.token)
        self._clients[info.pid] = client
        return client

    def _resolve(self, session):
        return pick_session(live_sessions(), session)

    def list_buffers(self, session) -> dict:
        client = self._client(self._resolve(session))
        symbols, generation = client.list_symbols()
        return {'symbols': symbols, 'stop_generation': generation}

    def fetch(self, session, symbol):
        """Return (meta, array), served from the per-stop cache."""
        info = self._resolve(session)
        client = self._client(info)
        generation = client.ping()
        key = (info.pid, symbol, generation)
        cached = self._cache.get(key)
        if cached is not None:
            return cached
        meta, raw = client.get_buffer(symbol, max_bytes=self._max_bytes)
        arr = decode_buffer(meta, raw)
        # Store under the same key we looked up (the ping generation), not
        # meta['stop_generation']: if the stop advanced between ping and
        # get_buffer those differ, and keying the entry off the lookup value
        # is what guarantees the next same-stop fetch finds it.
        self._cache.put(key, (meta, arr))
        return meta, arr

    def plot(self, session, symbol) -> None:
        self._client(self._resolve(session)).plot(symbol)


_INSTRUCTIONS = (
    'EXPERIMENTAL. This server exposes image buffers from a live gdb/lldb '
    'debug session — i.e. debuggee memory. Treat what you read as '
    'sensitive, local-only data.\n'
    'Token usage: `view` returns a rendered PNG and buffers can be large, '
    'so responses may be token-heavy. Keep output small: bound `view` with '
    '`region`, `channel`, and `max_px`; use `stats` for summaries and '
    '`values` for a small exact crop (capped at 1024 numbers) rather than '
    'reading whole buffers; use `dump` to write a lossless .npy to disk '
    'instead of returning bulk data through the conversation.'
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
def list_sessions() -> list[dict]:
    """List live OID debug sessions this server can reach."""
    return [{'pid': s.pid, 'debugger': s.debugger,
             'started_unix': s.start_time} for s in live_sessions()]


@mcp.tool()
def list_buffers(session: int | None = None) -> dict:
    """List symbols observable as image buffers at the current stop.

    session: debugger pid from list_sessions(); defaults to the most
    recently started live session.
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


def main() -> None:
    sys.stderr.write(_STARTUP_WARNING)
    sys.stderr.flush()
    mcp.run()


if __name__ == '__main__':
    main()
