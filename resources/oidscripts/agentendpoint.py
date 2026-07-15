# -*- coding: utf-8 -*-

"""
Debugger-side control endpoint for agent access.

Runs a stdlib-only localhost TCP server inside the debugger's embedded
Python. Socket threads never touch debugger APIs: every request is
wrapped in a closure and marshalled onto the debugger thread through
bridge.queue_request(), the same mechanism the viewer event loop uses.
"""

# Framing is defined once in wireframe. send_frame/recv_frame are used by
# the server below and re-exported so the endpoint's public surface
# (agentendpoint.send_frame, .recv_frame) is unchanged.
from oidscripts.wireframe import recv_frame, send_frame  # noqa: F401

PROTOCOL_VERSION = 1

ERROR_BAD_TOKEN = 'bad_token'
ERROR_UNKNOWN_METHOD = 'unknown_method'
ERROR_SYMBOL_NOT_FOUND = 'symbol_not_found'
ERROR_NOT_STOPPED = 'not_stopped'
ERROR_TOO_LARGE = 'too_large'
ERROR_TIMEOUT = 'timeout'
ERROR_NO_VIEWER = 'no_viewer'
ERROR_INTERNAL = 'internal'

import os
import secrets
import threading

from oidscripts.debuggers.interfaces import BufferTooLargeError

DEFAULT_REQUEST_TIMEOUT = 15.0

# An unauthenticated connection must complete its hello within this
# window; the timeout is lifted once the client has authenticated.
HANDSHAKE_TIMEOUT = 10.0

# Ceiling on simultaneously served connections; excess ones are closed.
MAX_CLIENTS = 8


def _endpoint_max_bytes():
    """
    Server-side ceiling on a single get_buffer read. Overridable with
    OID_AGENT_MAX_BYTES; parsed defensively so a bad value never breaks the
    endpoint. A client may only request a *smaller* cap, never remove it.
    """
    raw = os.environ.get('OID_AGENT_MAX_BYTES')
    if raw:
        try:
            value = int(raw)
            if value > 0:
                return value
        except ValueError:
            pass
    return 256 * 1024 * 1024


ENDPOINT_MAX_BYTES = _endpoint_max_bytes()


class EndpointError(Exception):
    """Structured protocol error carried back to the client."""

    def __init__(self, code, message):
        super(EndpointError, self).__init__('%s: %s' % (code, message))
        self.code = code
        self.message = message


def _classify(error):
    """
    Map an exception raised on the debugger thread to an EndpointError.
    Debuggers phrase 'target is running' errors differently but all
    known messages contain the word 'running'.
    """
    if isinstance(error, EndpointError):
        return error
    if isinstance(error, BufferTooLargeError):
        return EndpointError(ERROR_TOO_LARGE, str(error))
    if 'running' in str(error).lower():
        return EndpointError(ERROR_NOT_STOPPED,
                             'debuggee is not stopped: %s' % error)
    return EndpointError(ERROR_INTERNAL, str(error))


class AgentEndpoint(object):
    """
    Dispatches control-protocol requests. Debugger work is marshalled
    to the debugger thread via bridge.queue_request; this class is
    debugger-agnostic and unit-testable with a fake bridge.
    """

    def __init__(self, bridge, window, token,
                 request_timeout=DEFAULT_REQUEST_TIMEOUT):
        self._bridge = bridge
        self._window = window
        self.token = token
        self._timeout = request_timeout
        self._stop_generation = 0
        self._backend = bridge.get_backend_name()
        self._handlers = {
            'hello': self._handle_hello,
            'ping': self._handle_ping,
            'list_symbols': self._handle_list_symbols,
            'get_buffer': self._handle_get_buffer,
            'plot': self._handle_plot,
        }

    def notify_stop(self):
        self._stop_generation += 1

    def hello_response(self):
        return {
            'version': PROTOCOL_VERSION,
            'debugger': self._backend,
            'pid': os.getpid(),
            'target': None,
            'stop_generation': self._stop_generation,
        }

    def handle_request(self, request):
        """
        Handle one request dict; return (response_dict, payload_bytes).
        Raises EndpointError for protocol-level failures.
        """
        if not isinstance(request, dict):
            raise EndpointError(ERROR_UNKNOWN_METHOD,
                                'malformed request (not a JSON object)')
        method = request.get('method')
        if not isinstance(method, str):
            # An unhashable method (list/dict) must not become a
            # TypeError from the handler-table lookup.
            raise EndpointError(ERROR_UNKNOWN_METHOD,
                                'unknown method: %r' % (method,))
        handler = self._handlers.get(method)
        if handler is None:
            raise EndpointError(ERROR_UNKNOWN_METHOD,
                                'unknown method: %r' % (method,))
        return handler(request)

    def _run_in_debugger(self, fn):
        """
        Execute fn() on the debugger thread and return its result.
        """
        done = threading.Event()
        box = {}

        def task():
            try:
                box['result'] = fn()
            except Exception as error:  # forwarded to the client
                box['error'] = error
            finally:
                done.set()

        self._bridge.queue_request(task)
        if not done.wait(self._timeout):
            raise EndpointError(
                ERROR_TIMEOUT,
                'debugger did not answer within %gs' % self._timeout)
        if 'error' in box:
            raise _classify(box['error'])
        return box['result']

    def _handle_hello(self, request):
        supplied = str(request.get('token', ''))
        if not secrets.compare_digest(supplied, self.token):
            raise EndpointError(ERROR_BAD_TOKEN, 'bad or missing token')
        return self.hello_response(), b''

    def _handle_ping(self, request):
        return {'stop_generation': self._stop_generation}, b''

    def _handle_list_symbols(self, request):
        symbols = self._run_in_debugger(
            lambda: list(self._bridge.get_available_symbols()))
        return {'symbols': symbols,
                'stop_generation': self._stop_generation}, b''

    def _handle_get_buffer(self, request):
        symbol = str(request.get('symbol', ''))
        # Always enforce the server-side cap. A client may lower it, but a
        # missing/invalid value does NOT disable the guard (a direct
        # endpoint client must not be able to force unbounded reads).
        requested = request.get('max_bytes')
        if isinstance(requested, int) and 0 < requested < ENDPOINT_MAX_BYTES:
            max_bytes = requested
        else:
            max_bytes = ENDPOINT_MAX_BYTES

        def fetch():
            return self._bridge.get_buffer_metadata(symbol,
                                                    max_bytes=max_bytes)

        metadata = self._run_in_debugger(fetch)
        if metadata is None:
            raise EndpointError(
                ERROR_SYMBOL_NOT_FOUND,
                '%r is not an observable buffer in the current frame'
                % symbol)
        # Avoid copying the whole buffer inside the debugger process: the
        # bridge already materialized it into a bytes-like object (lldb:
        # bytes; gdb: a read_memory memoryview), and send_frame/sendall
        # accept the buffer protocol directly.
        pointer = metadata.pop('pointer')
        if not isinstance(pointer, (bytes, bytearray, memoryview)):
            pointer = memoryview(pointer)
        response = dict(metadata)
        response['stop_generation'] = self._stop_generation
        return response, pointer

    def _handle_plot(self, request):
        symbol = str(request.get('symbol', ''))

        def do_plot():
            if not self._window.is_ready():
                raise EndpointError(ERROR_NO_VIEWER,
                                    'no viewer window is connected')
            if not self._window.plot_variable(symbol):
                raise EndpointError(ERROR_INTERNAL,
                                    'viewer refused to plot %r' % symbol)
            return True

        self._run_in_debugger(do_plot)
        return {'ok': True}, b''


import atexit
import getpass
import json
import socket
import tempfile
import time

from oidscripts.logger import log


def discovery_dir():
    """
    Directory holding per-session discovery files. Overridable with
    OID_AGENT_DIR; defaults to <tempdir>/oid-agent-<user>.
    """
    override = os.environ.get('OID_AGENT_DIR')
    if override:
        return override
    try:
        user = getpass.getuser()
    except Exception:
        user = str(os.getuid()) if hasattr(os, 'getuid') else 'user'
    return os.path.join(tempfile.gettempdir(), 'oid-agent-%s' % user)


def _prepare_discovery_dir():
    path = discovery_dir()
    if os.path.islink(path):
        raise RuntimeError('discovery directory %s is a symlink' % path)
    if not os.path.isdir(path):
        os.makedirs(path, mode=0o700, exist_ok=True)
    if hasattr(os, 'getuid'):
        info = os.stat(path)
        if info.st_uid != os.getuid():
            raise RuntimeError(
                'discovery directory %s is owned by another user' % path)
        os.chmod(path, 0o700)
    return path


def _write_discovery_file(port, token, backend):
    directory = _prepare_discovery_dir()
    path = os.path.join(directory, '%d.json' % os.getpid())
    payload = json.dumps({
        'version': PROTOCOL_VERSION,
        'port': port,
        'token': token,
        'debugger': backend,
        'pid': os.getpid(),
        'start_time': time.time(),
    })
    # Use a unique temp name (mkstemp, mode 0600) so a stale <pid>.json.tmp
    # left by a crashed run can never wedge startup with EEXIST.
    fd, tmp = tempfile.mkstemp(dir=directory, prefix='%d.' % os.getpid(),
                               suffix='.tmp')
    try:
        os.write(fd, payload.encode('utf-8'))
    finally:
        os.close(fd)
    os.replace(tmp, path)
    return path


class _EndpointServer(object):
    """Owns the listening socket, client threads and discovery file."""

    def __init__(self, endpoint):
        self._endpoint = endpoint
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.bind(('127.0.0.1', 0))
        self._sock.listen(4)
        self._clients = []
        self._lock = threading.Lock()
        self.port = self._sock.getsockname()[1]
        try:
            self.discovery_path = _write_discovery_file(
                self.port, endpoint.token,
                endpoint.hello_response()['debugger'])
        except Exception:
            self._sock.close()
            raise
        self._thread = threading.Thread(
            target=self._accept_loop, name='oid-agent-endpoint', daemon=True)
        self._thread.start()

    def _accept_loop(self):
        while True:
            try:
                conn, _ = self._sock.accept()
            except OSError:
                return  # listening socket closed by close()
            try:
                with self._lock:
                    if len(self._clients) >= MAX_CLIENTS:
                        conn.close()
                        continue
                    self._clients.append(conn)
                thread = threading.Thread(
                    target=self._serve_client, args=(conn,),
                    name='oid-agent-client', daemon=True)
                thread.start()
            except Exception:
                # A per-connection failure (e.g. thread.start()) drops
                # that connection but must never kill the accept loop.
                with self._lock:
                    if conn in self._clients:
                        self._clients.remove(conn)
                try:
                    conn.close()
                except OSError:
                    pass  # already closed

    def _serve_client(self, conn):
        try:
            with conn:
                # Pre-auth: an idle connection may not hold its slot
                # open; it must say hello within the handshake window.
                conn.settimeout(HANDSHAKE_TIMEOUT)
                request, _ = recv_frame(conn, max_payload=0)
                try:
                    if (not isinstance(request, dict)
                            or request.get('method') != 'hello'):
                        raise EndpointError(ERROR_BAD_TOKEN,
                                            'first request must be hello')
                    response, payload = self._endpoint.handle_request(request)
                except EndpointError as error:
                    send_frame(conn, {'error': {'code': error.code,
                                                'message': error.message}})
                    return  # unauthenticated: drop the connection
                send_frame(conn, response, payload)
                # Authenticated: the client may idle between requests.
                conn.settimeout(None)
                while True:
                    request, _ = recv_frame(conn, max_payload=0)
                    try:
                        response, payload = \
                            self._endpoint.handle_request(request)
                        send_frame(conn, response, payload)
                    except EndpointError as error:
                        send_frame(conn,
                                   {'error': {'code': error.code,
                                              'message': error.message}})
        except (ValueError, OSError):
            # OSError covers ConnectionError; client went away or spoke
            # garbage. Nothing to clean up.
            pass
        finally:
            with self._lock:
                if conn in self._clients:
                    self._clients.remove(conn)

    def close(self):
        try:
            # Wake a thread blocked in accept() so the loop can exit;
            # on Linux close() alone does not interrupt accept().
            self._sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass  # socket not connected / already shut down
        try:
            self._sock.close()
        except OSError:
            pass  # already closed
        with self._lock:
            clients = list(self._clients)
        for conn in clients:
            try:
                conn.close()
            except OSError:
                pass  # client already disconnected
        try:
            os.remove(self.discovery_path)
        except OSError:
            pass  # discovery file already gone
        self._thread.join(timeout=2.0)
        if self._thread.is_alive():
            log.debug('agent endpoint accept thread did not exit within 2s')


_active = None


def start(bridge, window):
    """
    Start the module-level endpoint server. Returns the discovery file
    path, or None when the server is already running.
    """
    global _active
    if _active is not None:
        return None
    endpoint = AgentEndpoint(bridge, window, token=secrets.token_hex(32))
    server = _EndpointServer(endpoint)
    _active = (endpoint, server)
    atexit.register(shutdown)
    log.info('agent endpoint listening on 127.0.0.1:%d' % server.port)
    return server.discovery_path


def notify_stop():
    """Bump the stop generation. No-op when the endpoint is not running."""
    active = _active
    if active is not None:
        active[0].notify_stop()


def is_running():
    return _active is not None


def shutdown():
    """Stop the server and remove the discovery file. Idempotent."""
    global _active
    if _active is None:
        return
    _, server = _active
    _active = None
    server.close()
