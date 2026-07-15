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
