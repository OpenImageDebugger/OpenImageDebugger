import json
import os
import secrets
import socket
import sys
import tempfile
import threading
import time
from pathlib import Path

# Make `import oidscripts.agentendpoint` work: the endpoint lives in the
# sibling debugger-scripts tree, not in this package. Append (lowest
# precedence) to avoid shadowing similarly named modules on the path.
RESOURCES_DIR = str(Path(__file__).resolve().parents[2])
if RESOURCES_DIR not in sys.path:
    sys.path.append(RESOURCES_DIR)

from oidscripts import wireframe as wf
from oidscripts.debuggers.interfaces import raise_if_too_large


class FakeBridge:
    """Synchronous stand-in for GdbBridge/LldbBridge in tests."""

    def __init__(self, symbols=(), buffers=None, backend='gdb',
                 error=None, dead=False):
        self.symbols = list(symbols)
        self.buffers = dict(buffers or {})
        self.backend = backend
        self.error = error          # raised by get_buffer_metadata if set
        self.dead = dead            # queued requests are never executed
        self.pending = []
        self.fetch_count = 0

    def queue_request(self, callable_request):
        if self.dead:
            self.pending.append(callable_request)
        else:
            callable_request()

    def get_backend_name(self):
        return self.backend

    def get_available_symbols(self):
        return list(self.symbols)

    def get_buffer_metadata(self, variable, max_bytes=None):
        self.fetch_count += 1
        if self.error is not None:
            raise self.error
        if variable not in self.buffers:
            return None
        meta = dict(self.buffers[variable])
        raise_if_too_large(len(meta['pointer']), max_bytes)
        return meta


class FakeWindow:
    def __init__(self, ready=False, plot_result=1):
        self._ready = ready
        self._plot_result = plot_result
        self.plotted = []

    def is_ready(self):
        return self._ready

    def plot_variable(self, name):
        self.plotted.append(name)
        return self._plot_result


def make_meta(width, height, channels=1, type_value=5, row_stride=None,
              pixel_layout='rgba', transpose_buffer=False, raw=None):
    """Build a bridge-style metadata dict with 'pointer' holding bytes."""
    itemsize = {0: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 8}[type_value]
    stride = row_stride if row_stride is not None else width
    if raw is None:
        raw = bytes(height * stride * channels * itemsize)
    return {
        'display_name': 'fake',
        'pointer': raw,
        'width': width,
        'height': height,
        'channels': channels,
        'type': type_value,
        'row_stride': stride,
        'pixel_layout': pixel_layout,
        'transpose_buffer': transpose_buffer,
        'variable_name': 'fake',
    }


import numpy as np
import pytest

from oidscripts import agentendpoint


def gradient_meta():
    """4x3 float32 single-channel gradient buffer, values row*10+col."""
    arr = (np.arange(3)[:, None] * 10 + np.arange(4)[None, :]) \
        .astype(np.float32)
    return make_meta(4, 3, channels=1, type_value=5, raw=arr.tobytes())


@pytest.fixture
def dump_dir(tmp_path, monkeypatch):
    """Resolve the hardened per-user dump directory under tmp_path.

    gettempdir() caches its result in tempfile.tempdir; clearing the
    cache makes the patched TMPDIR take effect now, and monkeypatch
    restores the previous cache afterwards.
    """
    monkeypatch.setenv('TMPDIR', str(tmp_path))
    monkeypatch.setattr(tempfile, 'tempdir', None)
    return tmp_path


@pytest.fixture
def live_endpoint(tmp_path, monkeypatch):
    """Real agentendpoint server backed by a FakeBridge."""
    from oidmcp import discovery

    monkeypatch.setenv('OID_AGENT_DIR', str(tmp_path / 'agent'))
    bridge = FakeBridge(symbols=['img', 'grad'],
                        buffers={'grad': gradient_meta()})
    agentendpoint.start(bridge, FakeWindow(ready=True))
    sessions = discovery.live_sessions()
    assert len(sessions) == 1
    yield sessions[0], bridge
    agentendpoint.shutdown()


def viewer_gradient_buffer():
    """4x3 float32 single-channel gradient (values row*10+col), in the
    viewer's own wire shape (see oidmcp.viewer_meta.to_bridge_meta)."""
    arr = (np.arange(3)[:, None] * 10 + np.arange(4)[None, :]) \
        .astype(np.float32)
    meta = {
        'width': 4, 'height': 3, 'channels': 1, 'step': 4, 'type': 5,
        'pixel_layout': '', 'transpose_buffer': False,
        'display_name': 'grad',
    }
    return meta, arr.tobytes()


class _FakeViewerEndpoint:
    """Rogue socket + thread standing in for the C++ AgentCore in tests.

    Speaks the control-protocol wire format (``oidscripts.wireframe``)
    and answers ``hello``/``ping``/``list_buffers``/``get_buffer``/
    ``get_view``/``set_view`` from a canned buffer table and a mutable
    view state, so `SessionManager` viewer routing and tools can be
    driven end-to-end without a C++ build.
    """

    def __init__(self, buffers=None):
        self.buffers = buffers or {'grad': viewer_gradient_buffer()}
        self.view_state = {
            'buffer': None, 'center': None, 'zoom': None,
            'rotation_deg': None, 'channel': None, 'auto_contrast': False,
        }
        # Post-hello wire requests served, across all connections; lets
        # tests assert the warm path is ping-free (one request per call).
        self.requests_served = 0
        self.token = secrets.token_hex(32)
        self._listener = socket.socket()
        self._listener.bind(('127.0.0.1', 0))
        self._listener.listen(4)
        self.port = self._listener.getsockname()[1]
        # Every accepted connection is tracked so `close()` can shut them
        # all down, not just the listener: `SessionManager` connects a
        # fresh `ControlClient` per call and closes it right after, so no
        # test code holds a reference to any of these sockets. Without
        # tracking them here, a stray open connection surviving past this
        # fixture's teardown would let a later test's endpoint (a new
        # `_FakeViewerEndpoint` on a new port, but possibly the same
        # declared pid if the global `_manager` singleton is reused
        # across tests) be silently reachable through this dead socket
        # instead of a clean, freshly resolved connection.
        self._conns_lock = threading.Lock()
        self._conns = []
        self._thread = threading.Thread(target=self._accept_loop,
                                        daemon=True)
        self._thread.start()

    def _accept_loop(self):
        while True:
            try:
                conn, _ = self._listener.accept()
            except OSError:
                return  # listener closed: shut down cleanly
            with self._conns_lock:
                self._conns.append(conn)
            try:
                self._serve_connection(conn)
            except (ValueError, OSError):
                pass  # client went away or spoke garbage; nothing to clean up
            finally:
                with self._conns_lock:
                    if conn in self._conns:
                        self._conns.remove(conn)
                conn.close()

    def _serve_connection(self, conn):
        request, _ = wf.recv_frame(conn)  # hello
        if (request.get('method') != 'hello'
                or request.get('token') != self.token):
            wf.send_frame(conn, {'error': {'code': 'bad_token',
                                           'message': 'bad or missing '
                                                      'token'}})
            return
        wf.send_frame(conn, {'version': 1, 'kind': 'viewer', 'pid': 0})
        while True:
            request, _ = wf.recv_frame(conn)
            self._handle(conn, request)

    def _handle(self, conn, request):
        self.requests_served += 1
        method = request.get('method')
        handler = {
            'ping': self._handle_ping,
            'list_buffers': self._handle_list_buffers,
            'get_buffer': self._handle_get_buffer,
            'get_view': self._handle_get_view,
            'set_view': self._handle_set_view,
        }.get(method)
        if handler is None:
            wf.send_frame(conn, {'error': {'code': 'unknown_method',
                                           'message': 'unknown method: '
                                                      f'{method}'}})
            return
        handler(conn, request)

    def _handle_ping(self, conn, _request):
        # Matches the real AgentCore: no 'stop_generation' (a viewer has
        # no stop notion), unlike the debugger endpoint's ping reply.
        wf.send_frame(conn, {'ok': True})

    def _handle_list_buffers(self, conn, _request):
        entries = [{'name': name, 'display_name': meta['display_name'],
                   'width': meta['width'], 'height': meta['height'],
                   'channels': meta['channels'], 'type': meta['type'],
                   'pixel_layout': meta['pixel_layout']}
                  for name, (meta, _raw) in self.buffers.items()]
        wf.send_frame(conn, {'buffers': entries})

    def _handle_get_buffer(self, conn, request):
        name = request.get('symbol')
        entry = self.buffers.get(name)
        if entry is None:
            wf.send_frame(conn, {'error': {'code': 'unknown_buffer',
                                           'message': 'no such buffer: '
                                                      f'{name}'}})
            return
        meta, raw = entry
        wf.send_frame(conn, dict(meta), payload=raw)

    def _handle_get_view(self, conn, _request):
        wf.send_frame(conn, dict(self.view_state))

    def _handle_set_view(self, conn, request):
        for key in ('buffer', 'center', 'zoom', 'rotation_deg', 'channel',
                   'auto_contrast'):
            if key in request:
                self.view_state[key] = request[key]
        wf.send_frame(conn, dict(self.view_state))

    def close(self):
        # Wake a thread blocked in accept() so it can exit; on Linux
        # close() alone does not interrupt accept() (mirrors
        # agentendpoint._EndpointServer.close()).
        try:
            self._listener.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass  # already closed or never connected; nothing to wake
        self._listener.close()
        # Also force-close every still-open accepted connection (see the
        # comment in __init__): connect-per-call means each call opens and
        # closes its own connection, so nothing here is pooled -- this just
        # stops this fixture's teardown from leaving a leftover socket that
        # a later same-pid test could otherwise reach.
        with self._conns_lock:
            conns = list(self._conns)
        for conn in conns:
            try:
                conn.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass  # peer already gone; nothing to shut down
            conn.close()
        self._thread.join(timeout=2)


class FakeViewer:
    """A live fake viewer's discovery identity, plus its endpoint."""

    def __init__(self, pid, port, token, start_time, debugger_pid, endpoint):
        self.pid = pid
        self.port = port
        self.token = token
        self.start_time = start_time
        self.debugger_pid = debugger_pid
        self.endpoint = endpoint

    def close(self):
        self.endpoint.close()


def start_fake_viewer(agent_dir, pid=None, debugger_pid=None,
                      start_time=None, buffers=None):
    """
    Start one fake viewer endpoint and publish its discovery file.

    ``agent_dir`` is the ``OID_AGENT_DIR`` (the debugger discovery
    dir); the file is written under its ``viewer/`` subdirectory, like
    the real C++ ``AgentServer``. The viewer's own declared pid
    defaults to this test process's pid, mirroring how `live_endpoint`
    stands in for the debugger side. Unlike production, the discovery
    filename is keyed by port rather than pid, so multiple fake viewers
    sharing a declared pid within one test never collide on disk --
    ``oidmcp.discovery.live_viewers`` parses file *contents*, never
    names, so this is harmless.
    """
    endpoint = _FakeViewerEndpoint(buffers)
    resolved_pid = pid if pid is not None else os.getpid()
    resolved_start = start_time if start_time is not None else time.time()
    viewer_dir = Path(agent_dir) / 'viewer'
    viewer_dir.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(str(viewer_dir), 0o700)
    payload = {
        'pid': resolved_pid, 'port': endpoint.port, 'token': endpoint.token,
        'start_time': resolved_start,
    }
    if debugger_pid is not None:
        payload['debugger_pid'] = debugger_pid
    (viewer_dir / f'{endpoint.port}.json').write_text(json.dumps(payload))
    return FakeViewer(pid=resolved_pid, port=endpoint.port,
                      token=endpoint.token, start_time=resolved_start,
                      debugger_pid=debugger_pid, endpoint=endpoint)


@pytest.fixture
def live_viewer(tmp_path, monkeypatch):
    """
    Factory for fake viewer endpoints sharing one ``OID_AGENT_DIR``.

    Returns ``make_viewer(**kwargs)`` (see `start_fake_viewer`); every
    viewer it starts is torn down at fixture finalization. A single
    call with no arguments gives the common case: one live viewer
    answering hello/list_buffers/get_buffer/get_view/set_view from a
    canned gradient buffer, so tools can be driven end-to-end without a
    C++ build. Shares its ``OID_AGENT_DIR`` value with `live_endpoint`
    (both derive it from the same ``tmp_path``), so a test using both
    fixtures sees its debugger session and viewer window in one
    discovery directory, as in a real paired session.
    """
    agent_dir = tmp_path / 'agent'
    monkeypatch.setenv('OID_AGENT_DIR', str(agent_dir))
    viewers = []

    def make_viewer(**kwargs):
        viewer = start_fake_viewer(agent_dir, **kwargs)
        viewers.append(viewer)
        return viewer

    yield make_viewer

    for viewer in viewers:
        viewer.close()
