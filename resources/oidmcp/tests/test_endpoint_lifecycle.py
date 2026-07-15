import json
import os
import socket
import stat
import sys
import threading
import time
from pathlib import Path

import pytest

from oidscripts import agentendpoint as ep
from conftest import FakeBridge, FakeWindow


@pytest.fixture
def endpoint_session(tmp_path, monkeypatch):
    monkeypatch.setenv('OID_AGENT_DIR', str(tmp_path / 'agent'))
    bridge = FakeBridge(symbols=['img'])
    path = ep.start(bridge, FakeWindow())
    yield path, bridge
    ep.shutdown()


def _connect(path):
    info = json.loads(Path(path).read_text())
    sock = socket.create_connection(('127.0.0.1', info['port']), timeout=5)
    sock.settimeout(5)
    return sock, info


def test_start_writes_discovery_file(endpoint_session):
    path, _ = endpoint_session
    info = json.loads(Path(path).read_text())
    assert info['version'] == ep.PROTOCOL_VERSION
    assert info['pid'] == os.getpid()
    assert info['debugger'] == 'gdb'
    assert isinstance(info['port'], int)
    assert len(info['token']) >= 32
    assert os.path.basename(path) == '%d.json' % os.getpid()


@pytest.mark.skipif(sys.platform == 'win32', reason='POSIX permissions')
def test_discovery_artifacts_are_private(endpoint_session):
    path, _ = endpoint_session
    assert stat.S_IMODE(os.stat(path).st_mode) == 0o600
    assert stat.S_IMODE(os.stat(os.path.dirname(path)).st_mode) == 0o700


def test_hello_then_request_over_real_socket(endpoint_session):
    path, _ = endpoint_session
    sock, info = _connect(path)
    with sock:
        ep.send_frame(sock, {'method': 'hello', 'token': info['token']})
        response, _ = ep.recv_frame(sock)
        assert response['debugger'] == 'gdb'
        ep.send_frame(sock, {'method': 'list_symbols'})
        response, _ = ep.recv_frame(sock)
        assert response['symbols'] == ['img']


def test_first_frame_must_authenticate(endpoint_session):
    path, _ = endpoint_session
    sock, _ = _connect(path)
    with sock:
        ep.send_frame(sock, {'method': 'hello', 'token': 'wrong'})
        response, _ = ep.recv_frame(sock)
        assert response['error']['code'] == ep.ERROR_BAD_TOKEN
        # server closes the connection after a failed hello
        with pytest.raises(ConnectionError):
            ep.recv_frame(sock)


def test_non_dict_first_frame_drops_connection(endpoint_session):
    path, _ = endpoint_session
    sock, _ = _connect(path)
    with sock:
        # A non-object frame is a malformed-wire error: recv_frame rejects
        # it and the server drops the connection without a reply, the same
        # as any other unparseable frame (no error frame is sent).
        ep.send_frame(sock, [1])
        with pytest.raises(ConnectionError):
            ep.recv_frame(sock)


def test_error_responses_keep_connection_alive(endpoint_session):
    path, _ = endpoint_session
    sock, info = _connect(path)
    with sock:
        ep.send_frame(sock, {'method': 'hello', 'token': info['token']})
        ep.recv_frame(sock)
        ep.send_frame(sock, {'method': 'get_buffer', 'symbol': 'nope'})
        response, _ = ep.recv_frame(sock)
        assert response['error']['code'] == ep.ERROR_SYMBOL_NOT_FOUND
        ep.send_frame(sock, {'method': 'ping'})
        response, _ = ep.recv_frame(sock)
        assert response == {'stop_generation': 0}


def test_unauthenticated_idle_connection_is_dropped(endpoint_session,
                                                    monkeypatch):
    # Pre-auth connections must not linger: the endpoint applies a
    # handshake timeout until the hello frame arrives.
    monkeypatch.setattr(ep, 'HANDSHAKE_TIMEOUT', 0.2)
    path, _ = endpoint_session
    sock, _ = _connect(path)
    with sock:
        with pytest.raises(ConnectionError):
            ep.recv_frame(sock)


def test_authenticated_client_may_idle_past_handshake_timeout(
        endpoint_session, monkeypatch):
    # After a successful hello the timeout is lifted: a pooled client
    # idling between requests must not be dropped.
    monkeypatch.setattr(ep, 'HANDSHAKE_TIMEOUT', 0.2)
    path, _ = endpoint_session
    sock, info = _connect(path)
    with sock:
        ep.send_frame(sock, {'method': 'hello', 'token': info['token']})
        ep.recv_frame(sock)
        time.sleep(0.6)
        ep.send_frame(sock, {'method': 'ping'})
        response, _ = ep.recv_frame(sock)
        assert response == {'stop_generation': 0}


def test_excess_connections_are_refused(endpoint_session, monkeypatch):
    monkeypatch.setattr(ep, 'MAX_CLIENTS', 1)
    path, _ = endpoint_session
    first, info = _connect(path)
    with first:
        second, _ = _connect(path)
        with second:
            # Over the cap: the endpoint closes the excess connection.
            with pytest.raises(ConnectionError):
                ep.recv_frame(second)
        # The accept loop survives and the admitted client still works.
        ep.send_frame(first, {'method': 'hello', 'token': info['token']})
        response, _ = ep.recv_frame(first)
        assert response['debugger'] == 'gdb'


def test_module_notify_stop_reaches_endpoint(endpoint_session):
    path, _ = endpoint_session
    ep.notify_stop()
    sock, info = _connect(path)
    with sock:
        ep.send_frame(sock, {'method': 'hello', 'token': info['token']})
        response, _ = ep.recv_frame(sock)
        assert response['stop_generation'] == 1


def test_start_twice_is_noop(endpoint_session):
    _, bridge = endpoint_session
    assert ep.start(bridge, FakeWindow()) is None
    assert ep.is_running()


def test_shutdown_removes_discovery_file(tmp_path, monkeypatch):
    monkeypatch.setenv('OID_AGENT_DIR', str(tmp_path / 'agent'))
    path = ep.start(FakeBridge(), FakeWindow())
    assert os.path.exists(path)
    ep.shutdown()
    assert not os.path.exists(path)
    assert not ep.is_running()
    ep.shutdown()  # idempotent


@pytest.mark.skipif(sys.platform == 'win32', reason='POSIX symlinks')
def test_start_rejects_symlinked_discovery_dir(tmp_path, monkeypatch):
    real_dir = tmp_path / 'real-agent'
    real_dir.mkdir(mode=0o700)
    link = tmp_path / 'agent-link'
    os.symlink(str(real_dir), str(link))
    monkeypatch.setenv('OID_AGENT_DIR', str(link))
    bridge, window = FakeBridge(), FakeWindow()
    with pytest.raises(RuntimeError):
        ep.start(bridge, window)


def test_shutdown_joins_accept_thread(tmp_path, monkeypatch):
    monkeypatch.setenv('OID_AGENT_DIR', str(tmp_path / 'agent'))
    ep.start(FakeBridge(), FakeWindow())
    ep.shutdown()
    names = [t.name for t in threading.enumerate()]
    assert 'oid-agent-endpoint' not in names


def test_notify_stop_without_start_is_noop():
    # With no endpoint running, notify_stop must be a harmless no-op
    # and must not bring an endpoint into existence.
    assert not ep.is_running()
    ep.notify_stop()
    assert not ep.is_running()
