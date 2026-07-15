"""
Client side of the private control protocol spoken by the in-debugger
agent endpoint.

Frame encode/decode is shared with the endpoint through
oidscripts.wireframe (single source of the wire format).
oidmcp/__init__.py puts resources/ on sys.path so this import resolves
at runtime when oid-mcp runs from the OID source tree.
"""

from __future__ import annotations

import socket

from oidscripts.wireframe import recv_frame, send_frame


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
        # than requested. The endpoint rejects buffers above the cap, so a
        # legitimate payload is always <= max_bytes.
        meta, payload = self._call(request, max_payload=max_bytes)
        meta.pop('payload', None)
        return meta, payload

    def plot(self, symbol: str) -> None:
        self._call({'method': 'plot', 'symbol': symbol})

    def close(self) -> None:
        try:
            self._sock.close()
        except OSError:
            pass  # best-effort cleanup; the socket may already be closed
