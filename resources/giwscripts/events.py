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

    def stop_handler(self, event):
        """
        The debugger has stopped (e.g. a breakpoint was hit). We must list all
        available buffers and pass it to the imagewatch window.
        """
        # Block until the window is up and running
        if not self._window.is_running():
            self._window.initialize_window()
            while not self._window.is_running():
                time.sleep(0.1)

        observable_symbols = self._debugger.get_available_symbols()
        if self._window.is_running():
            self._window.update_available_variables(observable_symbols)

    def plot_handler(self, variable_name):
        """
        Command window to plot variable_name if user requests from debugger log
        """
        self._window.plot_variable(variable_name)
