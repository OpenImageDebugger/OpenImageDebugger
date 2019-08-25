# -*- coding: utf-8 -*-

"""
Implementation of handlers for events raised by the debugger
"""

import time

from giwscripts.debuggers.interfaces import BridgeEventHandlerInterface


class GdbImageWatchEvents(BridgeEventHandlerInterface):
    """
    Handles events raised by the debugger bridge
    """
    def __init__(self, window, debugger):
        self._window = window
        self._debugger = debugger

    def _set_symbol_complete_list(self):
        """
        Retrieve the list of available symbols and provide it to the GIW window
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
        available buffers and pass it to the imagewatch window.
        """
        # Block until the window is up and running
        if not self._window.is_ready():
            self._window.initialize_window()
            while not self._window.is_ready():
                time.sleep(0.1)

        # Update buffers being visualized
        observed_buffers = self._window.get_observed_buffers()
        for buffer_name in observed_buffers:
            self._window.plot_variable(buffer_name)

        # Set list of available symbols
        self._set_symbol_complete_list()

    def plot_handler(self, variable_name):
        """
        Command window to plot variable_name if user requests from debugger log
        """
        self._window.plot_variable(variable_name)
