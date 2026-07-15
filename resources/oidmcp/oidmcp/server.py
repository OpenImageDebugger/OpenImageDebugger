"""The oid-mcp stdio server: MCP tools over a live OID debug session."""

from __future__ import annotations

import json
import os

try:
    from mcp.server.mcpserver import MCPServer, Image
except ImportError:  # older SDK naming
    from mcp.server.fastmcp import FastMCP as MCPServer, Image

from .analysis import compute_stats, dump_npy, extract_values
from .buffers import BufferCache, decode_buffer
from .discovery import NoSessionError, live_sessions, pick_session
from .protocol import ControlClient, ControlError
from .render import render_view

DEFAULT_MAX_BYTES = 256 * 1024 * 1024

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


class SessionManager:
    """Connection pool + per-stop buffer cache over live sessions."""

    def __init__(self):
        self._clients: dict[int, ControlClient] = {}
        self._cache = BufferCache(capacity=4)
        self._max_bytes = int(
            os.environ.get('OID_MCP_MAX_BYTES', DEFAULT_MAX_BYTES))

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
        self._cache.put((info.pid, symbol, meta['stop_generation']),
                        (meta, arr))
        return meta, arr

    def plot(self, session, symbol) -> None:
        self._client(self._resolve(session)).plot(symbol)


mcp = MCPServer('oid')
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
def dump(symbol: str, path: str | None = None,
         session: int | None = None) -> str:
    """Save the buffer losslessly as .npy; returns the file path."""
    meta, arr = _fetch_or_fail(_manager, session, symbol)
    return dump_npy(arr, symbol, meta.get('stop_generation', 0),
                    path=path)


@mcp.tool()
def plot(symbol: str, session: int | None = None) -> str:
    """Mirror a buffer into the human's OID viewer window."""
    return _plot_impl(_manager, symbol, session)


def main() -> None:
    mcp.run()


if __name__ == '__main__':
    main()
