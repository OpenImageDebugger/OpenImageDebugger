# -*- coding: utf-8 -*-

"""
Implementation of handlers for events raised by the debugger
"""

import time

from oidscripts.debuggers.interfaces import BridgeEventHandlerInterface


class OpenImageDebuggerEvents(BridgeEventHandlerInterface):
    """
    Handles events raised by the debugger bridge
    """
    def __init__(self, window, debugger):
        self._window = window
        self._debugger = debugger

    def _set_symbol_complete_list(self):
        """
        Retrieve the list of available symbols and provide it to the OID window
        for autocompleting.
        """
        # #region agent log
        import json
        import time
        log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
        log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "events.py:25", "message": "_set_symbol_complete_list_entry", "data": {}, "timestamp": int(time.time() * 1000)}) + "\n")
        log_file.close()
        # #endregion
        observable_symbols = list(self._debugger.get_available_symbols())
        # #region agent log
        log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
        log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "events.py:28", "message": "after_get_available_symbols", "data": {"symbols_count": len(observable_symbols)}, "timestamp": int(time.time() * 1000)}) + "\n")
        log_file.close()
        # #endregion
        if self._window.is_ready():
            # #region agent log
            log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
            log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "events.py:30", "message": "before_set_available_symbols", "data": {}, "timestamp": int(time.time() * 1000)}) + "\n")
            log_file.close()
            # #endregion
            self._window.set_available_symbols(observable_symbols)
            # #region agent log
            log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
            log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "events.py:33", "message": "after_set_available_symbols", "data": {}, "timestamp": int(time.time() * 1000)}) + "\n")
            log_file.close()
            # #endregion

    def exit_handler(self):
        self._window.terminate()

    def stop_handler(self):
        """
        The debugger has stopped (e.g. a breakpoint was hit). We must list all
        available buffers and pass it to the Open Image Debugger window.
        """
        # FIXED: Window should already be initialized from main thread.
        # If not ready, wait a bit for it to become ready (it's being initialized
        # from main thread). This avoids calling initialize_window() from the
        # event loop thread which causes GIL errors.
        if not self._window.is_ready():
            # Wait for window to become ready (initialized from main thread)
            max_wait = 50  # 5 seconds max wait
            waited = 0
            while not self._window.is_ready() and waited < max_wait:
                time.sleep(0.1)
                waited += 1

        # Update buffers being visualized
        # H18: In LLDB mode, get_observed_buffers returns None (cannot create
        # Python objects). Check for None before iterating.
        # #region agent log
        import json
        import os
        log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
        log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "events.py:52", "message": "before_get_observed_buffers", "data": {}, "timestamp": int(time.time() * 1000)}) + "\n")
        log_file.close()
        # #endregion
        observed_buffers = self._window.get_observed_buffers()
        # #region agent log
        log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
        log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "events.py:55", "message": "after_get_observed_buffers", "data": {"observed_buffers_is_none": observed_buffers is None, "observed_buffers_type": str(type(observed_buffers)), "observed_buffers_repr": repr(observed_buffers)}, "timestamp": int(time.time() * 1000)}) + "\n")
        log_file.close()
        # #endregion
        if observed_buffers is not None:
            for buffer_name in observed_buffers:
                self._window.plot_variable(buffer_name)

        # Set list of available symbols
        # #region agent log
        log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
        log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "events.py:66", "message": "before_set_symbol_complete_list", "data": {}, "timestamp": int(time.time() * 1000)}) + "\n")
        log_file.close()
        # #endregion
        self._set_symbol_complete_list()
        # #region agent log
        log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
        log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "events.py:72", "message": "after_set_symbol_complete_list", "data": {}, "timestamp": int(time.time() * 1000)}) + "\n")
        log_file.close()
        # #endregion

    def plot_handler(self, variable_name):
        """
        Command window to plot variable_name if user requests from debugger log
        """
        self._window.plot_variable(variable_name)
