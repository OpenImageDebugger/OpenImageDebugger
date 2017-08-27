import ctypes
import os
import pysigset
import signal
import sys
import threading

class GdbImageWatchWindow():
    """
    Python interface for the imagewatch window, which is implemented as a
    shared library.
    """
    def __init__(self, bridge):
        self._bridge = bridge

        # Load imagewatch library and set up its API
        script_path = os.path.dirname(os.path.realpath(__file__))
        self._lib = ctypes.cdll.LoadLibrary(script_path+'/../libgdb-imagewatch.so')
        self._lib.plot_binary.argtypes = [
            ctypes.py_object, # Buffer ptr
            ctypes.py_object, # Variable name
            ctypes.c_int,     # Buffer width
            ctypes.c_int,     # Buffer height
            ctypes.c_int,     # Number of channels
            ctypes.c_int,     # Type (0=float32, 1=uint8)
            ctypes.c_int,     # Step size (in pixels)
            ctypes.py_object  # Pixel format
        ]
        self._FETCH_BUFFER_CBK_TYPE = ctypes.CFUNCTYPE(ctypes.c_int,
                                                       ctypes.c_char_p)
        self._lib.initialize_window.argtypes = [
            self._FETCH_BUFFER_CBK_TYPE # Python function to be called when the
                                        # user requests a symbol name from the
                                        # viewer interface
        ]

        self._lib.update_available_variables.argtypes = [
            ctypes.py_object # List of available variables in the current
                             # context
        ]

    def plot_variable(self, requested_symbol):
        if self._bridge is None:
            print('[gdb-imagewatch] Could not plot symbol %s: Not a debugging'
                    ' session.' % requested_symbol)
            return 0

        try:
            variable = requested_symbol.decode('utf-8')
            plot_callable = DeferredVariablePlotter(variable,
                                                    self._lib,
                                                    self._bridge)
            self._bridge.queue_request(plot_callable)
        except Exception as err:
            print('[gdb-imagewatch] Error: Could not plot variable')
            print(err)
            pass

        return 0

    def is_running(self):
        return self._lib.is_running()

    def update_available_variables(self, observable_symbols):
        self._lib.update_available_variables(observable_symbols)

    def initialize_window(self):
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
                args=(self._FETCH_BUFFER_CBK_TYPE(self.plot_variable),)
            )
            wnd_thread_instance.daemon = True
            wnd_thread_instance.start()
            pass
        pass


class DeferredVariablePlotter():
    """
    Instances of this class are callable objects whose __call__ method triggers
    a buffer plot command. Useful for deferring the plot command to a safe
    thread.
    """
    def __init__(self, variable, lib, bridge):
        self._variable = variable;
        self._lib = lib
        self._bridge = bridge
        pass
    def __call__(self):
        try:
            mem, width, height, channels, type, step, pixel_layout = self._bridge.get_buffer_metadata(self._variable)

            self._lib.plot_binary(mem, self._variable, width, height, channels, type, step, pixel_layout)
        except Exception as err:
            import traceback
            print('gdb-imagewatch] Error: Could not plot variable')
            print(err)
            traceback.print_exc()
        pass
    pass

