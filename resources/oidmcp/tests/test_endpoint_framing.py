import socket

import pytest

from oidscripts import wireframe as ep


def test_agentendpoint_reexports_shared_framing():
    from oidscripts import agentendpoint
    assert agentendpoint.send_frame is ep.send_frame
    assert agentendpoint.recv_frame is ep.recv_frame


def test_frame_roundtrip_without_payload():
    a, b = socket.socketpair()
    with a, b:
        ep.send_frame(a, {'method': 'ping'})
        obj, payload = ep.recv_frame(b)
    assert obj == {'method': 'ping'}
    assert payload == b''


def test_frame_roundtrip_with_payload():
    a, b = socket.socketpair()
    raw = bytes(range(256)) * 4
    with a, b:
        ep.send_frame(a, {'ok': True}, payload=raw)
        obj, payload = ep.recv_frame(b)
    assert obj['ok'] is True
    assert obj['payload'] == len(raw)
    assert payload == raw


def test_send_frame_does_not_mutate_caller_dict():
    a, b = socket.socketpair()
    obj = {'ok': True}
    with a, b:
        ep.send_frame(a, obj, payload=b'xyz')
    assert 'payload' not in obj


def test_recv_frame_rejects_oversized_json_frame():
    a, b = socket.socketpair()
    with a, b:
        a.sendall(ep._HEADER.pack(ep.MAX_FRAME_BYTES + 1))
        with pytest.raises(ValueError):
            ep.recv_frame(b)


def test_recv_frame_raises_on_peer_close():
    a, b = socket.socketpair()
    with b:
        a.close()
        with pytest.raises(ConnectionError):
            ep.recv_frame(b)


def test_recv_frame_rejects_oversized_payload():
    a, b = socket.socketpair()
    with a, b:
        ep.send_frame(a, {'ok': True}, payload=b'x' * 100)
        with pytest.raises(ValueError):
            ep.recv_frame(b, max_payload=10)


def test_recv_frame_allows_within_limit_payload():
    a, b = socket.socketpair()
    raw = b'y' * 10
    with a, b:
        ep.send_frame(a, {'ok': True}, payload=raw)
        _, payload = ep.recv_frame(b, max_payload=10)
    assert payload == raw


def test_recv_frame_rejects_negative_payload():
    import json
    a, b = socket.socketpair()
    with a, b:
        data = json.dumps({'ok': True, 'payload': -1}).encode('utf-8')
        a.sendall(ep._HEADER.pack(len(data)) + data)
        with pytest.raises(ValueError):
            ep.recv_frame(b)


def test_recv_frame_rejects_non_object_frame():
    import json
    for body in (5, [1, 2], 'payload'):
        a, b = socket.socketpair()
        with a, b:
            data = json.dumps(body).encode('utf-8')
            a.sendall(ep._HEADER.pack(len(data)) + data)
            with pytest.raises(ValueError):
                ep.recv_frame(b)
