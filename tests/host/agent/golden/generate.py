# -*- coding: utf-8 -*-

"""
Generate the golden wire-frame fixtures shared by the C++ and Python
wire-codec tests (see ../wire_frame_test.cpp and
resources/oidmcp/tests/test_golden_frames.py).

Every fixture is produced by the real oidscripts.wireframe encoder --
captured through a fake sendall()-only socket -- so the bytes on disk are
canonical Python wire frames, not hand-assembled ones. manifest.json then
records what wireframe.recv_frame decodes those same bytes into, so both
language test suites can assert against one source of truth instead of
duplicating the encoder's 'payload' field injection logic.

This script is stdlib-only (plus oidscripts.wireframe) and deterministic:
no timestamps, no randomness. It is run by hand, once, and its outputs
(ping.bin, ok_payload.bin, manifest.json) are committed; the test suites
only ever read those committed files.
"""

import json
import sys
from pathlib import Path

_GOLDEN_DIR = Path(__file__).resolve().parent
# tests/host/agent/golden/generate.py -> repo root
_REPO_ROOT = _GOLDEN_DIR.parents[3]
_RESOURCES_DIR = _REPO_ROOT / 'resources'
if str(_RESOURCES_DIR) not in sys.path:
    sys.path.insert(0, str(_RESOURCES_DIR))

from oidscripts import wireframe  # noqa: E402


class _SendCapture:
    """sendall()-only sink: captures exactly the bytes send_frame writes."""

    def __init__(self):
        self.data = bytearray()

    def sendall(self, chunk):
        self.data.extend(chunk)


class _RecvPlayback:
    """recv_into()-only source: replays already-encoded bytes for decode."""

    def __init__(self, data):
        self._buf = memoryview(data)

    def recv_into(self, view):
        n = min(len(view), len(self._buf))
        view[:n] = self._buf[:n]
        self._buf = self._buf[n:]
        return n


def _encode(obj, payload=b''):
    sink = _SendCapture()
    wireframe.send_frame(sink, obj, payload=payload)
    return bytes(sink.data)


def _decode(data, max_payload):
    return wireframe.recv_frame(_RecvPlayback(data), max_payload=max_payload)


def _build_fixture(filename, obj, payload):
    """Encode obj+payload, write the wire bytes, and describe the decode."""
    wire_bytes = _encode(obj, payload=payload)
    decoded_obj, decoded_payload = _decode(wire_bytes, max_payload=len(payload))
    assert decoded_payload == payload

    (_GOLDEN_DIR / filename).write_bytes(wire_bytes)

    return {
        'decoded_obj': decoded_obj,
        'payload_len': len(payload),
        'payload_hex': payload.hex(),
    }


def main():
    manifest = {
        'ping.bin': _build_fixture('ping.bin', {'method': 'ping'}, b''),
        'ok_payload.bin': _build_fixture(
            'ok_payload.bin', {'ok': True}, bytes(range(8))),
    }
    manifest_text = json.dumps(manifest, indent=4, sort_keys=True) + '\n'
    (_GOLDEN_DIR / 'manifest.json').write_text(manifest_text)


if __name__ == '__main__':
    main()
