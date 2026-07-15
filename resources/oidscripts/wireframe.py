# -*- coding: utf-8 -*-

"""
Shared, stdlib-only frame encode/decode for the agent control protocol.

Single source of the wire framing: imported both by the in-debugger
endpoint (oidscripts.agentendpoint) and by the oid-mcp client
(oidmcp.protocol). Stdlib-only so it loads inside whatever Python the
debugger embeds.

Wire format: 4-byte big-endian length prefix + UTF-8 JSON object. A
frame whose JSON carries 'payload': <nbytes> is immediately followed by
that many raw bytes (used for pixel data, which never goes through
JSON).
"""

import json
import struct

_HEADER = struct.Struct('>I')

# Sanity bound for JSON frames only; binary payloads are sized by the
# 'payload' field and bounded by the get_buffer max_bytes parameter.
MAX_FRAME_BYTES = 1 << 20


def send_frame(sock, obj, payload=b''):
    """
    Send a JSON frame, optionally followed by a raw binary payload.
    """
    if payload:
        obj = dict(obj)
        obj['payload'] = len(payload)
    data = json.dumps(obj).encode('utf-8')
    sock.sendall(_HEADER.pack(len(data)) + data)
    if payload:
        sock.sendall(payload)


def _recv_exact(sock, size):
    chunks = []
    remaining = size
    while remaining > 0:
        chunk = sock.recv(min(remaining, 65536))
        if not chunk:
            raise ConnectionError('peer closed the connection')
        chunks.append(chunk)
        remaining -= len(chunk)
    return b''.join(chunks)


def recv_frame(sock):
    """
    Receive one frame; return (json_dict, payload_bytes).
    """
    (length,) = _HEADER.unpack(_recv_exact(sock, _HEADER.size))
    if length > MAX_FRAME_BYTES:
        raise ValueError('JSON frame too large: %d bytes' % length)
    obj = json.loads(_recv_exact(sock, length).decode('utf-8'))
    payload = b''
    if 'payload' in obj:
        payload = _recv_exact(sock, int(obj['payload']))
    return obj, payload
