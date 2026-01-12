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

        In LLDB mode, this queues the call to run on the main thread to avoid
        GIL issues when calling Python C API functions from the event loop
        thread.
        """
        observable_symbols = list(self._debugger.get_available_symbols())

        # Check if we're in LLDB mode
        # In LLDB mode, set_available_symbols() eventually calls
        # oid_set_available_symbols() which crashes if called from a non-main
        # thread. We need to queue it.
        is_lldb_mode = 'lldb' in sys.modules

        if is_lldb_mode:
            # Queue the call to run on the main thread via the event loop
            # This ensures it runs in a safe context where Python C API
            # calls work
            from oidscripts.oidwindow import DeferredSymbolListSetter
            deferred_setter = DeferredSymbolListSetter(
                self._window, observable_symbols)
            self._debugger.queue_request(deferred_setter)
        else:
            # Non-LLDB mode: can call directly
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
        with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
            f.write(json.dumps({"sessionId":"debug-session","runId":"run1","hypothesisId":"H5,H6","location":"events.py:54","message":"stop_handler_entry","data":{"window_ready":self._window.is_ready()},"timestamp":int(time.time()*1000)}) + '\n')
        # #endregion
        
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
        # In LLDB mode, get_observed_buffers now uses a Python-side cache
        # since C++ can't create Python objects. This cache is maintained
        # when buffers are plotted.
        observed_buffers = self._window.get_observed_buffers()
        # #region agent log
        with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
            f.write(json.dumps({"sessionId":"debug-session","runId":"run1","hypothesisId":"H6","location":"events.py:74","message":"got_observed_buffers","data":{"observed_buffers":observed_buffers if observed_buffers is None else list(observed_buffers),"buffer_count":len(observed_buffers) if observed_buffers is not None else 0},"timestamp":int(time.time()*1000)}) + '\n')
        # #endregion
        
        # Update all observed buffers (check for both None and empty list)
        if observed_buffers:
            for buffer_name in observed_buffers:
                # #region agent log
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({"sessionId":"debug-session","runId":"run1","hypothesisId":"H6","location":"events.py:87","message":"plotting_buffer","data":{"buffer_name":buffer_name},"timestamp":int(time.time()*1000)}) + '\n')
                # #endregion
                self._window.plot_variable(buffer_name)

        # Set list of available symbols
        # #region agent log
        with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
            f.write(json.dumps({"sessionId":"debug-session","runId":"run1","hypothesisId":"H6","location":"events.py:80","message":"calling_set_symbol_complete_list","data":{},"timestamp":int(time.time()*1000)}) + '\n')
        # #endregion
        self._set_symbol_complete_list()
        # #region agent log
        with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
            f.write(json.dumps({"sessionId":"debug-session","runId":"run1","hypothesisId":"H5,H6","location":"events.py:82","message":"stop_handler_exit","data":{},"timestamp":int(time.time()*1000)}) + '\n')
        # #endregion

    def plot_handler(self, variable_name):
        """
        Command window to plot variable_name if user requests from debugger log
        """
        self._window.plot_variable(variable_name)
