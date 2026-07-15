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
    # Fill a single preallocated buffer with recv_into rather than
    # accumulating a list of chunks and joining them, which would need a
    # second payload-sized allocation for large (up to the cap) transfers.
    if size == 0:
        return b''
    buf = bytearray(size)
    view = memoryview(buf)
    pos = 0
    while pos < size:
        received = sock.recv_into(view[pos:])
        if received == 0:
            raise ConnectionError('peer closed the connection')
        pos += received
    return bytes(buf)


def recv_frame(sock, max_payload=None):
    """
    Receive one frame; return (json_dict, payload_bytes).

    max_payload optionally caps the declared 'payload' size; a frame
    declaring more is rejected with ValueError before the payload bytes
    are read. Default None preserves unbounded payloads (needed by
    clients that receive large pixel buffers).
    """
    (length,) = _HEADER.unpack(_recv_exact(sock, _HEADER.size))
    if length > MAX_FRAME_BYTES:
        raise ValueError('JSON frame too large: %d bytes' % length)
    obj = json.loads(_recv_exact(sock, length).decode('utf-8'))
    if not isinstance(obj, dict):
        # A scalar or array frame would make the 'payload' membership
        # test / lookup below raise TypeError; reject it as a protocol
        # error the callers already handle.
        raise ValueError('frame is not a JSON object')
    payload = b''
    if 'payload' in obj:
        nbytes = int(obj['payload'])
        if nbytes < 0:
            raise ValueError('negative payload size: %d' % nbytes)
        if max_payload is not None and nbytes > max_payload:
            raise ValueError('payload of %d bytes exceeds limit %d'
                             % (nbytes, max_payload))
        payload = _recv_exact(sock, nbytes)
    return obj, payload
