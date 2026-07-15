"""
Client side of the private control protocol spoken by the in-debugger
agent endpoint.

Frame encode/decode is shared with the endpoint through
oidscripts/wireframe.py (single source of the wire format). oid-mcp runs
from the OID source tree; rather than mutating the interpreter-wide
sys.path, that stdlib-only module is loaded directly from its file so
importing this package stays free of side effects.
"""

from __future__ import annotations

import importlib.util as _importlib_util
import socket
from pathlib import Path as _Path


def _load_wireframe():
    """Load the shared framing module (`oidscripts/wireframe.py`) by path.

    The module is stdlib-only by design (so it also loads inside whatever
    Python the debugger embeds), which makes a file-path load safe without
    putting the sibling `oidscripts` tree on sys.path.
    """
    path = _Path(__file__).resolve().parents[2] / 'oidscripts' / 'wireframe.py'
    spec = _importlib_util.spec_from_file_location('oidmcp._wireframe', path)
    if spec is None or spec.loader is None:  # pragma: no cover - defensive
        raise ImportError(f'cannot load wire framing from {path}')
    module = _importlib_util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


_wireframe = _load_wireframe()
recv_frame = _wireframe.recv_frame
send_frame = _wireframe.send_frame

# Client-side ceiling on a single get_buffer payload, applied when the caller
# does not request a smaller cap. Matches the endpoint's own default ceiling so
# a legitimate buffer always fits, while still bounding how much a misbehaving
# endpoint can make us read. server.py imports this as its transfer-cap default.
DEFAULT_MAX_BYTES = 256 * 1024 * 1024


class ControlError(Exception):
    """Structured error returned by the endpoint."""

    def __init__(self, code: str, message: str):
        super().__init__(f'{code}: {message}')
        self.code = code
        self.message = message


class ControlClient:
    """One authenticated connection to an agent endpoint."""

    def __init__(self, host: str, port: int, token: str,
                 timeout: float = 20.0):
        self._sock = socket.create_connection((host, port), timeout=timeout)
        self._sock.settimeout(timeout)
        try:
            self.hello, _ = self._call({'method': 'hello', 'token': token})
        except BaseException:
            # A failed handshake (bad token, timeout, ...) must not leak the
            # socket we just opened.
            self.close()
            raise

    def _call(self, request: dict,
              max_payload: int | None = None) -> tuple[dict, bytes]:
        send_frame(self._sock, request)
        response, payload = recv_frame(self._sock, max_payload=max_payload)
        if 'error' in response:
            info = response['error']
            raise ControlError(info.get('code', 'internal'),
                               info.get('message', ''))
        return response, payload

    def ping(self) -> int:
        response, _ = self._call({'method': 'ping'})
        return response['stop_generation']

    def list_symbols(self) -> tuple[list[str], int]:
        response, _ = self._call({'method': 'list_symbols'})
        return response['symbols'], response['stop_generation']

    def get_buffer(self, symbol: str,
                   max_bytes: int | None = None) -> tuple[dict, bytes]:
        request = {'method': 'get_buffer', 'symbol': symbol}
        if max_bytes is not None:
            request['max_bytes'] = max_bytes
        # Enforce the cap on the client side too: bound how many payload bytes
        # we will read so a misbehaving endpoint can't make us buffer more
        # than requested. When the caller omits max_bytes we still apply a safe
        # default ceiling instead of reading unbounded. The endpoint rejects
        # buffers above the cap, so a legitimate payload always fits.
        limit = max_bytes if max_bytes is not None else DEFAULT_MAX_BYTES
        meta, payload = self._call(request, max_payload=limit)
        meta.pop('payload', None)
        return meta, payload

    def plot(self, symbol: str) -> None:
        self._call({'method': 'plot', 'symbol': symbol})

    def close(self) -> None:
        try:
            self._sock.close()
        except OSError:
            pass  # best-effort cleanup; the socket may already be closed
