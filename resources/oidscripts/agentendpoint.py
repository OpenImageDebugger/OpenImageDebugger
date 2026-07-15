# -*- coding: utf-8 -*-

"""
Debugger-side control endpoint for agent access.

Runs a stdlib-only localhost TCP server inside the debugger's embedded
Python. Socket threads never touch debugger APIs: every request is
wrapped in a closure and marshalled onto the debugger thread through
bridge.queue_request(), the same mechanism the viewer event loop uses.
"""

# Framing is defined once in wireframe and re-exported here so the
# endpoint's public surface (agentendpoint.send_frame, .recv_frame, ...)
# is unchanged.
from oidscripts.wireframe import (  # noqa: F401
    _HEADER,
    MAX_FRAME_BYTES,
    _recv_exact,
    recv_frame,
    send_frame,
)

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
        method = request.get('method')
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
        max_bytes = request.get('max_bytes')

        def fetch():
            return self._bridge.get_buffer_metadata(symbol,
                                                    max_bytes=max_bytes)

        metadata = self._run_in_debugger(fetch)
        if metadata is None:
            raise EndpointError(
                ERROR_SYMBOL_NOT_FOUND,
                '%r is not an observable buffer in the current frame'
                % symbol)
        payload = bytes(metadata.pop('pointer'))
        response = dict(metadata)
        response['stop_generation'] = self._stop_generation
        return response, payload

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
