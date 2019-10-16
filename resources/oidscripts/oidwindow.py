# -*- coding: utf-8 -*-
import array
from oidscripts import symbols

"""
Classes related to exposing an interface to the OpenImageDebugger window
"""

import ctypes
import ctypes.util
import platform
import sys
import time

FETCH_BUFFER_CBK_TYPE = ctypes.CFUNCTYPE(ctypes.c_int,
                                         ctypes.c_char_p)


class OpenImageDebuggerWindow(object):
    """
    Python interface for the OpenImageDebugger window, which is implemented as a
    shared library.
    """
    def __init__(self, script_path, bridge):
        self._bridge = bridge
        self._script_path = script_path

        # Request ctypes to load libGL before the native oidwindow does; this
        # fixes an issue on Ubuntu machines with nvidia drivers. For more
        # information, please refer to
        # https://github.com/csantosbh/gdb-imagewatch/issues/28
        ctypes.CDLL(ctypes.util.find_library('GL'), ctypes.RTLD_GLOBAL)

        # Load OpenImageDebugger library and set up its API
        self._lib = ctypes.cdll.LoadLibrary(
            script_path + '/' + OpenImageDebuggerWindow.__get_library_name())

        # lib OID API
        self._lib.oid_initialize.argtypes = [FETCH_BUFFER_CBK_TYPE, ctypes.py_object]
        self._lib.oid_initialize.restype = ctypes.c_void_p

        self._lib.oid_cleanup.argtypes = [ctypes.c_void_p]
        self._lib.oid_cleanup.restype = None

        self._lib.oid_exec.argtypes = [ctypes.c_void_p]
        self._lib.oid_exec.restype = None

        self._lib.oid_is_window_ready.argtypes = [ctypes.c_void_p]
        self._lib.oid_is_window_ready.restype = ctypes.c_bool

        self._lib.oid_get_observed_buffers.argtypes = [ctypes.c_void_p]
        self._lib.oid_get_observed_buffers.restype = ctypes.py_object

        self._lib.oid_set_available_symbols.argtypes = [
            ctypes.c_void_p,
            ctypes.py_object
        ]
        self._lib.oid_set_available_symbols.restype = None

        self._lib.oid_run_event_loop.argtypes = [ctypes.c_void_p]
        self._lib.oid_run_event_loop.restype = None

        self._lib.oid_plot_buffer.argtypes = [
            ctypes.c_void_p,
            ctypes.py_object
        ]
        self._lib.oid_plot_buffer.restype = None

        # UI handler
        self._native_handler = None
        self._event_loop_wait_time = 1.0/30.0
        self._previous_evloop_time = OpenImageDebuggerWindow.__get_time_ms()
        self._plot_variable_c_callback = FETCH_BUFFER_CBK_TYPE(self.plot_variable)


    @staticmethod
    def __get_time_ms():
        return int(time.time() * 1000.0)

    @staticmethod
    def __get_library_name():
        """
        Return the name of the binary library, including its extension, which
        is platform dependent.
        """
        platform_name = platform.system().lower()
        python_version = '_python%d' % sys.version_info[0]

        if platform_name == 'linux':
            return 'liboidbridge%s.so' % python_version
        elif platform_name == 'darwin':
            return 'liboidbridge%s.dylib' % python_version

    def plot_variable(self, requested_symbol):
        """
        Plot a variable whose name is 'requested_symbol'.

        This will result in the creation of a callable object of type
        DeferredVariablePlotter, where the actual code for plotting the buffer
        will be executed. This object will be given to the debugger bridge so
        that it can schedule its execution in a thread safe context.
        """
        if self._bridge is None:
            print('[OpenImageDebugger] Could not plot symbol %s: Not a debugging'
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
            print('[OpenImageDebugger] Error: Could not plot variable')
            print(err)

        return 0

    def is_ready(self):
        """
        Returns True if the OpenImageDebugger window has been loaded; False otherwise.
        """
        if self._native_handler is None:
            return False

        return self._lib.oid_is_window_ready(self._native_handler)

    def terminate(self):
        """
        Request OID to terminate application and close all windows
        """
        self._lib.oid_cleanup(self._native_handler)

    def set_available_symbols(self, observable_symbols):
        """
        Set the autocomplete list of symbols with the list of string
        'observable_symbols'
        """
        self._lib.oid_set_available_symbols(
            self._native_handler,
            observable_symbols)

    def run_event_loop(self):
        """
        Run the debugger-side event loop, which consists of checking for new
        user requests coming from the UI
        """
        current_time = OpenImageDebuggerWindow.__get_time_ms()
        dt = current_time - self._previous_evloop_time
        if dt > self._event_loop_wait_time:
            self._lib.oid_run_event_loop(self._native_handler)
            self._previous_evloop_time = current_time

        # Schedule next run of the event loop
        self._bridge.queue_request(self.run_event_loop)

    def get_observed_buffers(self):
        """
        Get a list with the currently observed symbols in the OID window
        """
        return self._lib.oid_get_observed_buffers(self._native_handler)

    def initialize_window(self):
        # Initialize OID lib
        self._native_handler = self._lib.oid_initialize(
            self._plot_variable_c_callback,
            {'oid_path': self._script_path})

        # Launch UI
        self._lib.oid_exec(self._native_handler)

        # Schedule event loop
        self._bridge.queue_request(self.run_event_loop)


class DeferredVariablePlotter(object):
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
                self._lib.oid_plot_buffer(
                    self._native_handler,
                    buffer_metadata)
        except Exception as err:
            import traceback
            print('[OpenImageDebugger] Error: Could not plot variable')
            print(err)
            traceback.print_exc()
