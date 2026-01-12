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

        # Update all observed buffers (check for both None and empty list)
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
