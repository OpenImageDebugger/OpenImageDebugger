# -*- coding: utf-8 -*-

"""
Cross-language golden-frame tests.

tests/host/agent/golden/*.bin are canonical wire bytes produced once by
oidscripts.wireframe (see tests/host/agent/golden/generate.py) and
committed alongside manifest.json, which documents what decoding each
fixture is expected to yield. tests/host/agent/wire_frame_test.cpp
asserts the C++ codec decodes those same bytes to the same fields and
round-trips through its own encoder; this module asserts the same of
oidscripts.wireframe, plus -- since the fixtures are themselves this
encoder's output -- that re-encoding reproduces them byte-for-byte.
Byte-identical output across the two encoders is NOT asserted anywhere:
nlohmann's dump() legitimately differs from json.dumps (whitespace, key
order), which is interop-irrelevant given length-prefixed framing.
"""

import json
from pathlib import Path

import pytest

from oidscripts import wireframe as wf

# resources/oidmcp/tests/test_golden_frames.py -> repo root
_GOLDEN_DIR = (Path(__file__).resolve().parents[3]
               / 'tests' / 'host' / 'agent' / 'golden')

_FIXTURE_NAMES = ['ping.bin', 'ok_payload.bin']


class _RecvPlayback:
    """recv_into()-only fake socket that replays fixed bytes for decode."""

    def __init__(self, data):
        self._buf = memoryview(data)

    def recv_into(self, view):
        n = min(len(view), len(self._buf))
        view[:n] = self._buf[:n]
        self._buf = self._buf[n:]
        return n


class _SendCapture:
    """sendall()-only fake socket that captures the bytes an encode writes."""

    def __init__(self):
        self.data = bytearray()

    def sendall(self, chunk):
        self.data.extend(chunk)


def _manifest():
    return json.loads((_GOLDEN_DIR / 'manifest.json').read_text())


def _golden_bytes(name):
    return (_GOLDEN_DIR / name).read_bytes()


def _encode_input_obj(entry):
    # decoded_obj already carries the 'payload' key send_frame injects
    # itself, so strip it back out before re-encoding.
    input_obj = dict(entry['decoded_obj'])
    input_obj.pop('payload', None)
    return input_obj


@pytest.mark.parametrize('name', _FIXTURE_NAMES)
def test_golden_fixture_decodes_to_manifest_fields(name):
    entry = _manifest()[name]
    expected_payload = bytes.fromhex(entry['payload_hex'])

    obj, payload = wf.recv_frame(_RecvPlayback(_golden_bytes(name)),
                                  max_payload=len(expected_payload))

    assert obj == entry['decoded_obj']
    assert payload == expected_payload


@pytest.mark.parametrize('name', _FIXTURE_NAMES)
def test_own_encoder_reproduces_fixture_bytes_exactly(name):
    entry = _manifest()[name]
    expected_payload = bytes.fromhex(entry['payload_hex'])

    sock = _SendCapture()
    wf.send_frame(sock, _encode_input_obj(entry), payload=expected_payload)

    assert bytes(sock.data) == _golden_bytes(name)


@pytest.mark.parametrize('name', _FIXTURE_NAMES)
def test_golden_fixture_round_trips_through_own_codec(name):
    entry = _manifest()[name]
    expected_payload = bytes.fromhex(entry['payload_hex'])

    sock = _SendCapture()
    wf.send_frame(sock, _encode_input_obj(entry), payload=expected_payload)
    obj, payload = wf.recv_frame(_RecvPlayback(bytes(sock.data)),
                                  max_payload=len(expected_payload))

    assert obj == entry['decoded_obj']
    assert payload == expected_payload
