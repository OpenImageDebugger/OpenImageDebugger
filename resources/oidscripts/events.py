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
        observable_symbols = list(self._debugger.get_available_symbols())
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
        # In LLDB mode, get_observed_buffers returns None (cannot create
        # Python objects). Check for None before iterating.
        observed_buffers = self._window.get_observed_buffers()
        if observed_buffers is not None:
            for buffer_name in observed_buffers:
                self._window.plot_variable(buffer_name)

        # Set list of available symbols
        self._set_symbol_complete_list()

    def plot_handler(self, variable_name):
        """
        Command window to plot variable_name if user requests from debugger log
        """
        self._window.plot_variable(variable_name)
