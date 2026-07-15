import numpy as np
import pytest

from oidmcp.protocol import ControlClient, ControlError


def make_client(session):
    return ControlClient('127.0.0.1', session.port, session.token)


def test_hello_handshake(live_endpoint):
    session, _ = live_endpoint
    client = make_client(session)
    assert client.hello['debugger'] == 'gdb'
    client.close()


def test_bad_token_raises(live_endpoint):
    session, _ = live_endpoint
    with pytest.raises(ControlError) as excinfo:
        ControlClient('127.0.0.1', session.port, 'wrong-token')
    assert excinfo.value.code == 'bad_token'


def test_list_symbols_and_ping(live_endpoint):
    session, _ = live_endpoint
    client = make_client(session)
    symbols, generation = client.list_symbols()
    assert symbols == ['img', 'grad']
    assert client.ping() == generation
    client.close()


def test_get_buffer_roundtrip(live_endpoint):
    session, _ = live_endpoint
    client = make_client(session)
    meta, raw = client.get_buffer('grad')
    assert (meta['width'], meta['height'], meta['type']) == (4, 3, 5)
    values = np.frombuffer(raw, dtype=np.float32).reshape(3, 4)
    assert values[2, 3] == pytest.approx(23.0)
    client.close()


def test_semantic_errors_carry_codes(live_endpoint):
    session, _ = live_endpoint
    client = make_client(session)
    with pytest.raises(ControlError) as excinfo:
        client.get_buffer('missing')
    assert excinfo.value.code == 'symbol_not_found'
    with pytest.raises(ControlError) as excinfo:
        client.get_buffer('grad', max_bytes=4)
    assert excinfo.value.code == 'too_large'
    # Ready viewer: plot succeeds and the connection stays usable
    # (an error would have raised ControlError instead).
    client.plot('grad')
    assert isinstance(client.ping(), int)
    client.close()


def test_client_bounds_received_payload_to_max_bytes():
    # Defense in depth: even if the endpoint misbehaves and sends more than
    # the requested cap, the client refuses to buffer past max_bytes.
    import socket
    import threading

    from oidscripts import wireframe as wf

    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
    port = listener.getsockname()[1]

    def rogue_endpoint():
        conn, _ = listener.accept()
        with conn:
            wf.recv_frame(conn)  # hello
            wf.send_frame(conn, {'version': 1, 'debugger': 'x',
                                 'stop_generation': 0})
            wf.recv_frame(conn)  # get_buffer
            wf.send_frame(conn, {'width': 1, 'stop_generation': 0},
                          payload=b'x' * 100)

    thread = threading.Thread(target=rogue_endpoint, daemon=True)
    thread.start()

    client = ControlClient('127.0.0.1', port, 'tok')
    with pytest.raises((ValueError, ControlError)):
        client.get_buffer('img', max_bytes=16)
    client.close()
    listener.close()
    thread.join(timeout=2)


def test_client_bounds_received_payload_without_explicit_max(monkeypatch):
    # Omitting max_bytes must NOT disable the client-side bound: get_buffer
    # falls back to a safe default ceiling so a misbehaving endpoint can't make
    # us buffer an unbounded payload.
    import socket
    import threading

    from oidmcp import protocol
    from oidscripts import wireframe as wf

    monkeypatch.setattr(protocol, 'DEFAULT_MAX_BYTES', 16)

    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
    port = listener.getsockname()[1]

    def rogue_endpoint():
        conn, _ = listener.accept()
        with conn:
            wf.recv_frame(conn)  # hello
            wf.send_frame(conn, {'version': 1, 'debugger': 'x',
                                 'stop_generation': 0})
            wf.recv_frame(conn)  # get_buffer
            wf.send_frame(conn, {'width': 1, 'stop_generation': 0},
                          payload=b'x' * 100)

    thread = threading.Thread(target=rogue_endpoint, daemon=True)
    thread.start()

    client = protocol.ControlClient('127.0.0.1', port, 'tok')
    with pytest.raises((ValueError, ControlError)):
        client.get_buffer('img')  # no max_bytes → default ceiling applies
    client.close()
    listener.close()
    thread.join(timeout=2)
