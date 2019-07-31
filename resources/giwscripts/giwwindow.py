# -*- coding: utf-8 -*-

"""
Classes related to exposing an interface to the ImageWatch window
"""

import ctypes
import ctypes.util
import signal
import threading
import platform
import sys

FETCH_BUFFER_CBK_TYPE = ctypes.CFUNCTYPE(ctypes.c_int,
                                         ctypes.c_char_p)


class GdbImageWatchWindow():
    """
    Python interface for the imagewatch window, which is implemented as a
    shared library.
    """
    def __init__(self, script_path, bridge):
        self._bridge = bridge

        # Request ctypes to load libGL before the native giwwindow does; this
        # fixes an issue on Ubuntu machines with nvidia drivers. For more
        # information, please refer to
        # https://github.com/csantosbh/gdb-imagewatch/issues/28
        ctypes.CDLL(ctypes.util.find_library('GL'), ctypes.RTLD_GLOBAL)

        # Load imagewatch library and set up its API
        self._lib = ctypes.cdll.LoadLibrary(
            script_path + '/' + GdbImageWatchWindow.__get_library_name())

        # libgiw API
        self._lib.giw_initialize.argtypes = []
        self._lib.giw_initialize.restype = ctypes.c_void_p

        self._lib.giw_cleanup.argtypes = [ctypes.c_void_p]
        self._lib.giw_cleanup.restype = None

        self._lib.giw_exec.argtypes = [ctypes.c_void_p]
        self._lib.giw_exec.restype = None

        self._lib.giw_is_window_ready.argtypes = [ctypes.c_void_p]
        self._lib.giw_is_window_ready.restype = ctypes.c_bool

        self._lib.giw_get_observed_buffers.argtypes = [ctypes.c_void_p]
        self._lib.giw_get_observed_buffers.restype = ctypes.py_object

        self._lib.giw_set_available_symbols.argtypes = [
            ctypes.c_void_p,
            ctypes.py_object
        ]
        self._lib.giw_set_available_symbols.restype = None

        self._lib.giw_plot_buffer.argtypes = [
            ctypes.c_void_p,
            ctypes.py_object
        ]
        self._lib.giw_plot_buffer.restype = None

        # UI handler
        self._native_handler = None

    @staticmethod
    def __get_library_name():
        """
        Return the name of the binary library, including its extension, which
        is platform dependent.
        """
        platform_name = platform.system().lower()
        python_version = '_python%d' % sys.version_info[0]

        if platform_name == 'linux':
            return 'libgiwbridge%s.so' % python_version
        elif platform_name == 'darwin':
            return 'libgiwbridge%s.dylib' % python_version

    def plot_variable(self, requested_symbol):
        """
        Plot a variable whose name is 'requested_symbol'.

        This will result in the creation of a callable object of type
        DeferredVariablePlotter, where the actual code for plotting the buffer
        will be executed. This object will be given to the debugger bridge so
        that it can schedule its execution in a thread safe context.
        """
        if self._bridge is None:
            print('[gdb-imagewatch] Could not plot symbol %s: Not a debugging'
                  ' session.' % requested_symbol)
            return 0

        try:
            if not isinstance(requested_symbol, str):
                variable = requested_symbol.decode('utf-8')
            else:
                variable = requested_symbol

            plot_callable = DeferredVariablePlotter(variable,
                                                    self._lib,
                                                    self._bridge,
                                                    self._native_handler)
            self._bridge.queue_request(plot_callable)
            return 1
        except Exception as err:
            print('[gdb-imagewatch] Error: Could not plot variable')
            print(err)

        return 0

    def is_ready(self):
        """
        Returns True if the ImageWatch window has been loaded; False otherwise.
        """
        if self._native_handler is None:
            return False

        return self._lib.giw_is_window_ready(self._native_handler)

    def terminate(self):
        """
        Request GIW to terminate application and close all windows
        """
        self._lib.giw_cleanup(self._native_handler)

    def set_available_symbols(self, observable_symbols):
        """
        Set the autocomplete list of symbols with the list of string
        'observable_symbols'
        """
        self._lib.giw_set_available_symbols(
            self._native_handler,
            observable_symbols)

    def get_observed_buffers(self):
        """
        Get a list with the currently observed symbols in the giw window
        """
        return self._lib.giw_get_observed_buffers(self._native_handler)

    def initialize_window(self):
        # Initialize GIW lib
        self._native_handler = self._lib.giw_initialize(
            FETCH_BUFFER_CBK_TYPE(self.plot_variable))
        # Launch UI
        self._lib.giw_exec(self._native_handler)


class DeferredVariablePlotter():
    """
    Instances of this class are callable objects whose __call__ method triggers
    a buffer plot command. Useful for deferring the plot command to a safe
    thread.
    """
    def __init__(self, variable, lib, bridge, native_handler):
        self._variable = variable
        self._lib = lib
        self._bridge = bridge
        self._native_handler = native_handler

    def __call__(self):
        try:
            buffer_metadata = self._bridge.get_buffer_metadata(self._variable)

            if buffer_metadata is not None:
                self._lib.giw_plot_buffer(
                    self._native_handler,
                    buffer_metadata)
        except Exception as err:
            import traceback
            print('[gdb-imagewatch] Error: Could not plot variable')
            print(err)
            traceback.print_exc()
