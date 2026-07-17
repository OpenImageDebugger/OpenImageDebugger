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


def test_set_view_cannot_override_method(live_endpoint):
    session, _ = live_endpoint
    client = make_client(session)
    captured = {}
    client._call = lambda request, **kw: (captured.update(request) or ({}, b''))
    # A caller-supplied 'method' must not replace the RPC method.
    client.set_view(method='evil', zoom=2.0)
    assert captured['method'] == 'set_view'
    client.close()


def test_json_only_call_rejects_declared_payload():
    # Control calls are JSON-only; a server declaring a binary payload must be
    # rejected (default max_payload=0), not allocated for.
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
            wf.recv_frame(conn)  # list_symbols
            wf.send_frame(conn, {'symbols': [], 'stop_generation': 0},
                          payload=b'x' * 100)

    thread = threading.Thread(target=rogue_endpoint, daemon=True)
    thread.start()

    client = ControlClient('127.0.0.1', port, 'tok')
    with pytest.raises(ValueError):
        client.list_symbols()
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


def _start_rogue_viewer(handle_request):
    """Spin up a one-shot fake viewer endpoint on a loopback socket.

    Speaks the hello handshake, then hands the next request's parsed JSON
    to ``handle_request`` and sends back whatever it returns as the
    response frame. Returns (port, thread, listener) for the caller to
    connect to and tear down.
    """
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
            request, _ = wf.recv_frame(conn)  # hello
            assert request.get('token') == 'tok'
            wf.send_frame(conn, {'version': 1, 'debugger': 'x',
                                  'stop_generation': 0})
            request, _ = wf.recv_frame(conn)
            response = handle_request(request)
            wf.send_frame(conn, response)

    thread = threading.Thread(target=rogue_endpoint, daemon=True)
    thread.start()
    return port, thread, listener


def test_get_view_roundtrip():
    canned_view = {
        'buffer': 'img', 'center': [1.5, 2.5], 'zoom': 2.0,
        'rotation_deg': 90, 'channel': -1, 'auto_contrast': False,
        'stop_generation': 0,
    }

    def handle_request(request):
        assert request['method'] == 'get_view'
        return canned_view

    port, thread, listener = _start_rogue_viewer(handle_request)
    client = ControlClient('127.0.0.1', port, 'tok')
    view = client.get_view()
    assert view == canned_view
    client.close()
    listener.close()
    thread.join(timeout=2)


def test_get_view_raises_control_error():
    def handle_request(request):
        assert request['method'] == 'get_view'
        return {'error': {'code': 'no_view', 'message': 'no active view'}}

    port, thread, listener = _start_rogue_viewer(handle_request)
    client = ControlClient('127.0.0.1', port, 'tok')
    with pytest.raises(ControlError) as excinfo:
        client.get_view()
    assert excinfo.value.code == 'no_view'
    client.close()
    listener.close()
    thread.join(timeout=2)


def test_set_view_sends_only_passed_fields():
    seen_requests = []

    def handle_request(request):
        seen_requests.append(request)
        return {'buffer': 'grad', 'center': [0.0, 0.0], 'zoom': 1.0,
                'rotation_deg': 0, 'channel': -1, 'auto_contrast': True,
                'stop_generation': 0}

    port, thread, listener = _start_rogue_viewer(handle_request)
    client = ControlClient('127.0.0.1', port, 'tok')
    result = client.set_view(buffer='grad', zoom=1.0)
    client.close()
    listener.close()
    thread.join(timeout=2)

    assert len(seen_requests) == 1
    request = seen_requests[0]
    assert request['method'] == 'set_view'
    assert request['buffer'] == 'grad'
    assert request['zoom'] == pytest.approx(1.0)
    assert 'center' not in request
    assert 'rotation_deg' not in request
    assert 'channel' not in request
    assert 'auto_contrast' not in request
    assert result['buffer'] == 'grad'


def test_set_view_raises_control_error():
    def handle_request(request):
        assert request['method'] == 'set_view'
        return {'error': {'code': 'bad_field', 'message': 'bad zoom'}}

    port, thread, listener = _start_rogue_viewer(handle_request)
    client = ControlClient('127.0.0.1', port, 'tok')
    with pytest.raises(ControlError) as excinfo:
        client.set_view(zoom=-1.0)
    assert excinfo.value.code == 'bad_field'
    client.close()
    listener.close()
    thread.join(timeout=2)


def test_list_viewer_buffers_roundtrip():
    canned_buffers = [{'name': 'img'}, {'name': 'grad'}]

    def handle_request(request):
        assert request['method'] == 'list_buffers'
        return {'buffers': canned_buffers, 'stop_generation': 0}

    port, thread, listener = _start_rogue_viewer(handle_request)
    client = ControlClient('127.0.0.1', port, 'tok')
    buffers = client.list_viewer_buffers()
    assert buffers == canned_buffers
    client.close()
    listener.close()
    thread.join(timeout=2)


def test_list_viewer_buffers_raises_control_error():
    def handle_request(request):
        assert request['method'] == 'list_buffers'
        return {'error': {'code': 'internal', 'message': 'boom'}}

    port, thread, listener = _start_rogue_viewer(handle_request)
    client = ControlClient('127.0.0.1', port, 'tok')
    with pytest.raises(ControlError) as excinfo:
        client.list_viewer_buffers()
    assert excinfo.value.code == 'internal'
    client.close()
    listener.close()
    thread.join(timeout=2)
