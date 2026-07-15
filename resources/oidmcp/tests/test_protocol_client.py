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
