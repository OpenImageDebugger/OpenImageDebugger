# -*- coding: utf-8 -*-

"""
Classes related to exposing an interface to the ImageWatch window
"""

import ctypes
import signal
import threading

import pysigset


FETCH_BUFFER_CBK_TYPE = ctypes.CFUNCTYPE(ctypes.c_int,
                                         ctypes.c_char_p)

class GdbImageWatchWindow():
    """
    Python interface for the imagewatch window, which is implemented as a
    shared library.
    """
    def __init__(self, script_path, bridge):
        self._bridge = bridge

        # Load imagewatch library and set up its API
        self._lib = ctypes.cdll.LoadLibrary(
            script_path + '/libgdb-imagewatch.so')
        self._lib.plot_binary.argtypes = [
            # Buffer pointer
            ctypes.py_object,
            # Variable name string
            ctypes.py_object,
            # Buffer width
            ctypes.c_int,
            # Buffer height
            ctypes.c_int,
            # Number of channels
            ctypes.c_int,
            # Type (see symbols.py for details)
            ctypes.c_int,
            # Buffer row stride size (in pixels)
            ctypes.c_int,
            # Pixel format string (e.g. 'rgba')
            ctypes.py_object
        ]
        self._lib.initialize_window.argtypes = [
            # Python function to be called when the user requests a symbol name
            # from the viewer interface
            FETCH_BUFFER_CBK_TYPE
        ]

        self._lib.update_available_variables.argtypes = [
            # List of available variables in the current context
            ctypes.py_object
        ]

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
                                                    self._bridge)
            self._bridge.queue_request(plot_callable)
        except Exception as err:
            print('[gdb-imagewatch] Error: Could not plot variable')
            print(err)

        return 0

    def is_running(self):
        """
        Returns True if the ImageWatch window is running; False otherwise.
        """
        return self._lib.is_running()

    def terminate(self):
        """
        Clean up and close window
        """
        return self._lib.terminate()

    def update_available_variables(self, observable_symbols):
        """
        Given a dict containing buffers that were fetched by the debugger
        bridge and considered as 'plottable' buffers by the giwtype.py script,
        this function will provide these elements to the ImageWatch window so
        that it can update the buffers being visualized, as well as populate
        its internal autocomplete list.
        """
        self._lib.update_available_variables(observable_symbols)

    def initialize_window(self):
        """
        Launch the ImageWatch window.
        """
        ##
        # Initialize imagewatch window
        with pysigset.suspended_signals(signal.SIGCHLD):
            # By default, my new threads will be the ones receiving the
            # precious signals from the operating system. These signals should
            # go to GDB so it could do its thing, and since it doesnt get them,
            # my thread will make gdb hang. The solution is to configure the
            # new thread to forward the signal back to the main thread, which
            # is done by pysigset.
            wnd_thread_instance = threading.Thread(
                target=self._lib.initialize_window,
                args=(FETCH_BUFFER_CBK_TYPE(self.plot_variable),)
            )
            wnd_thread_instance.daemon = True
            wnd_thread_instance.start()


class DeferredVariablePlotter():
    """
    Instances of this class are callable objects whose __call__ method triggers
    a buffer plot command. Useful for deferring the plot command to a safe
    thread.
    """
    def __init__(self, variable, lib, bridge):
        self._variable = variable
        self._lib = lib
        self._bridge = bridge

    def __call__(self):
        try:
            mem, width, height, channels, typevalue, step, pixel_layout = self._bridge.get_buffer_metadata(self._variable)

            self._lib.plot_binary(mem, self._variable, width, height, channels, typevalue, step, pixel_layout)
        except Exception as err:
            import traceback
            print('[gdb-imagewatch] Error: Could not plot variable')
            print(err)
            traceback.print_exc()
