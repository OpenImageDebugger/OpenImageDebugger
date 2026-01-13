# -*- coding: utf-8 -*-

"""
Implementation of handlers for events raised by the debugger
"""

import time
import sys

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
        import os
        log_path = "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log"
        try:
            with open(log_path, "a") as log:
                entry = {
                    "sessionId": "debug-session",
                    "runId": "run1",
                    "hypothesisId": "A",
                    "location": "events.py:21",
                    "message": "_set_symbol_complete_list entry",
                    "data": {},
                    "timestamp": int(time.time() * 1000)
                }
                log.write(json.dumps(entry) + "\n")
        except Exception:
            pass
        # #endregion

        observable_symbols = list(self._debugger.get_available_symbols())

        # #region agent log
        try:
            with open(log_path, "a") as log:
                entry = {
                    "sessionId": "debug-session",
                    "runId": "run1",
                    "hypothesisId": "A",
                    "location": "events.py:30",
                    "message": "_set_symbol_complete_list after get_available_symbols",
                    "data": {
                        "symbol_count": len(observable_symbols),
                        "first_5_symbols": ", ".join(list(observable_symbols)[:5])
                    },
                    "timestamp": int(time.time() * 1000)
                }
                log.write(json.dumps(entry) + "\n")
        except Exception:
            pass
        # #endregion

        if 'lldb' in sys.modules:
            from oidscripts.oidwindow import DeferredSymbolListSetter
            deferred_setter = DeferredSymbolListSetter(
                self._window, observable_symbols)
            self._debugger.queue_request(deferred_setter)
        else:
            if self._window.is_ready():
                self._window.set_available_symbols(observable_symbols)

    def exit_handler(self):
        self._window.terminate()

    def stop_handler(self):
        """
        The debugger has stopped (e.g. a breakpoint was hit). We must list all
        available buffers and pass it to the Open Image Debugger window.
        """
        # #region agent log
        import json
        import os
        log_path = "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log"
        try:
            with open(log_path, "a") as log:
                entry = {
                    "sessionId": "debug-session",
                    "runId": "run1",
                    "hypothesisId": "A",
                    "location": "events.py:40",
                    "message": "stop_handler called",
                    "data": {},
                    "timestamp": int(time.time() * 1000)
                }
                log.write(json.dumps(entry) + "\n")
        except Exception:
            pass
        # #endregion

        if not self._window.is_ready():
            max_wait = 50
            waited = 0
            while not self._window.is_ready() and waited < max_wait:
                time.sleep(0.1)
                waited += 1

        observed_buffers = self._window.get_observed_buffers()
        if observed_buffers:
            for buffer_name in observed_buffers:
                self._window.plot_variable(buffer_name)

        # Set list of available symbols
        self._set_symbol_complete_list()

    def plot_handler(self, variable_name):
        """
        Command window to plot variable_name if user requests from debugger log
        """
        self._window.plot_variable(variable_name)
