# -*- coding: utf-8 -*-

"""
Classes related to exposing an interface to the OpenImageDebugger window
"""

import ctypes
import ctypes.util
import os
import platform
import sys
import time

from oidscripts.logger import log

FETCH_BUFFER_CBK_TYPE = ctypes.CFUNCTYPE(ctypes.c_int,
                                         ctypes.c_char_p)


PLATFORM_NAME = platform.system().lower()

class OpenImageDebuggerWindow(object):
    """
    Python interface for the OpenImageDebugger window, which is implemented as a
    shared library.
    """
    def __init__(self, script_path, bridge):
        self._bridge = bridge
        self._script_path = script_path
        # Cache of observed buffer names for LLDB mode where get_observed_buffers() doesn't work
        self._observed_buffers_cache = set()

        if PLATFORM_NAME == 'windows':
            os.add_dll_directory(script_path)
            os.add_dll_directory(os.getenv('Qt5_Dir') + '/' + 'bin')

        # Request ctypes to load libGL before the native oidwindow does; this
        # fixes an issue on Ubuntu machines with nvidia drivers. For more
        # information, please refer to
        # https://github.com/csantosbh/gdb-imagewatch/issues/28
        lib_opengl = 'opengl32' if PLATFORM_NAME == 'windows' else 'GL'
        ctypes.CDLL(ctypes.util.find_library(lib_opengl), ctypes.RTLD_GLOBAL)

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

        # LLDB-safe version that takes C string array instead of Python list
        self._lib.oid_set_available_symbols_safe.argtypes = [
            ctypes.c_void_p,  # handler
            ctypes.POINTER(ctypes.c_char_p),  # available_vars array
            ctypes.c_size_t  # count
        ]
        self._lib.oid_set_available_symbols_safe.restype = None

        self._lib.oid_run_event_loop.argtypes = [ctypes.c_void_p]
        self._lib.oid_run_event_loop.restype = None

        self._lib.oid_plot_buffer.argtypes = [
            ctypes.c_void_p,
            ctypes.py_object
        ]
        self._lib.oid_plot_buffer.restype = None

#H19 : LLDB - safe version that takes individual parameters
        self._lib.oid_plot_buffer_safe.argtypes = [
            ctypes.c_void_p,  # handler
            ctypes.c_char_p,  # variable_name
            ctypes.c_char_p,  # display_name
            ctypes.c_void_p,  # buffer_ptr
            ctypes.c_size_t,  # buffer_size
            ctypes.c_int,  # width
            ctypes.c_int,  # height
            ctypes.c_int,  # channels
            ctypes.c_int,  # type
            ctypes.c_int,  # row_stride
            ctypes.c_char_p,  # pixel_layout
            ctypes.c_int,  # transpose_buffer
        ]
        self._lib.oid_plot_buffer_safe.restype = None

#UI handler
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
        if PLATFORM_NAME == 'linux' or PLATFORM_NAME == 'darwin':
            return 'liboidbridge.so'
        elif PLATFORM_NAME == 'windows':
            return 'oidbridge.dll'
        return None

    def plot_variable(self, requested_symbol):
        """
        Plot a variable whose name is 'requested_symbol'.

        This will result in the creation of a callable object of type
        DeferredVariablePlotter, where the actual code for plotting the buffer
        will be executed. This object will be given to the debugger bridge so
        that it can schedule its execution in a thread safe context.
        """
        if self._bridge is None:
            log.info(f"Could not plot symbol {requested_symbol}: Not a debugging session")
            return 0

        try:
            if not isinstance(requested_symbol, str):
                variable = requested_symbol.decode('utf-8')
            else:
                variable = requested_symbol

            # Add to cache immediately - will be removed if plotting fails
            self._observed_buffers_cache.add(variable)

            plot_callable = DeferredVariablePlotter(variable,
                                                    self._lib,
                                                    self._bridge,
                                                    self._native_handler,
                                                    self)  # Pass window reference

            self._bridge.queue_request(plot_callable)

            return 1
        except Exception as err:
            log.error('Could not plot variable')
            log.error(err)
            # Remove from cache if plotting failed
            if isinstance(requested_symbol, str):
                self._observed_buffers_cache.discard(requested_symbol)
            else:
                self._observed_buffers_cache.discard(requested_symbol.decode('utf-8'))

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
        import sys
        is_lldb_mode = 'lldb' in sys.modules

        # Perform two-level sorting:
        # 1. By amount of nodes.
        # 2. By variable name.
        sorted_observable_symbols = sorted(observable_symbols, key=lambda symbol: (symbol.count('.'), symbol))

        if is_lldb_mode:
            # In LLDB mode, use the safe version that takes C string array
            # instead of Python list to avoid Python C API calls from event loop thread
            import ctypes
            count = len(sorted_observable_symbols)
            
            # Encode all strings to bytes and keep references to prevent GC
            encoded_symbols = []
            for symbol in sorted_observable_symbols:
                encoded = symbol.encode('utf-8') if isinstance(symbol, str) else symbol
                encoded_symbols.append(encoded)
            
            # Create array of C strings
            c_string_array = (ctypes.c_char_p * count)()
            for i, encoded in enumerate(encoded_symbols):
                c_string_array[i] = encoded
            
            # Keep references to prevent garbage collection
            if not hasattr(self, '_symbol_array_refs'):
                self._symbol_array_refs = []
            self._symbol_array_refs.append(c_string_array)
            self._symbol_array_refs.append(encoded_symbols)
            
            self._lib.oid_set_available_symbols_safe(
                self._native_handler,
                c_string_array,
                count)
        else:
            # Non-LLDB mode: use original function with Python list
            self._lib.oid_set_available_symbols(
                self._native_handler,
                sorted_observable_symbols)

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
        
        In LLDB mode, oid_get_observed_buffers returns Py_None because Python
        objects can't be created. In this case, we fall back to the Python-side
        cache of observed buffers.
        """
        import sys
        is_lldb_mode = 'lldb' in sys.modules

        # In LLDB mode, use the cache since C++ can't return the list
        if is_lldb_mode:
            return list(self._observed_buffers_cache)

        # Non-LLDB mode: try to get from C++ side
        try:
            result = self._lib.oid_get_observed_buffers(self._native_handler)
            # If result is None or not a list, fall back to cache
            if result is None or not isinstance(result, list):
                return list(self._observed_buffers_cache)
            # Update cache with results from C++ side
            self._observed_buffers_cache = set(result)
            return result
        except Exception:
            # If call fails, fall back to cache
            return list(self._observed_buffers_cache)

    def initialize_window(self):
        # Initialize OID lib
        try:
            self._native_handler = self._lib.oid_initialize(
                self._plot_variable_c_callback,
                {'oid_path': self._script_path})
        except Exception:
            raise

        # Launch UI
        self._lib.oid_exec(self._native_handler)

        # Schedule event loop
        self._bridge.queue_request(self.run_event_loop)


class DeferredSymbolListSetter(object):
    """
    Instances of this class are callable objects whose __call__ method sets
    the available symbols list for autocompletion. Useful for deferring the
    symbol list update to a safe thread (main thread) to avoid GIL issues
    in LLDB mode.
    """
    def __init__(self, window, symbols):
        self._window = window
        self._symbols = symbols
    
    def __call__(self):
        try:
            if self._window.is_ready():
                self._window.set_available_symbols(self._symbols)
        except Exception as err:
            import traceback, logging
            log = logging.getLogger(__name__)
            log.error("Could not set available symbols")
            log.error(err)
            traceback.print_exc()


class DeferredVariablePlotter(object):
    """
    Instances of this class are callable objects whose __call__ method triggers
    a buffer plot command. Useful for deferring the plot command to a safe
    thread.
    """
    def __init__(self, variable, lib, bridge, native_handler, window=None):
        self._variable = variable
        self._lib = lib
        self._bridge = bridge
        self._native_handler = native_handler
        self._window = window  # Window reference for cache management

    def __call__(self):
        try:
            buffer_metadata = self._bridge.get_buffer_metadata(self._variable)

            if buffer_metadata is None:
                # Don't remove from cache if process is running - the buffer might
                # still be valid, we just can't read it right now. Only remove if
                # process is stopped and we still can't get metadata (variable doesn't exist).
                # This prevents the cache from being cleared when continuing quickly.
                import sys
                if 'lldb' in sys.modules:
                    import lldb
                    try:
                        # Try to get process state to determine if we should keep buffer in cache
                        debugger = self._bridge.get_lldb_backend()
                        if debugger and debugger.GetSelectedTarget().IsValid():
                            process = debugger.GetSelectedTarget().process
                            # Only remove from cache if process is stopped and metadata is still None
                            # If process is running, keep it in cache for next stop event
                            if process.IsValid() and process.GetState() == lldb.eStateStopped:
                                # Process is stopped but metadata is None - variable doesn't exist
                                if self._window is not None:
                                    self._window._observed_buffers_cache.discard(self._variable)
                            # If process is running, keep buffer in cache - don't remove it
                        else:
                            # Can't determine state - keep buffer in cache to be safe
                            pass
                    except Exception:
                        # If we can't check process state, keep buffer in cache to be safe
                        pass
                else:
                    # Non-LLDB mode: remove from cache on failure
                    if self._window is not None:
                        self._window._observed_buffers_cache.discard(self._variable)
                return

            # In LLDB mode, use the safe version that doesn't require
            # Python C API calls to parse the dictionary. Extract values in
            # Python and pass as individual arguments.
            import sys
            # Check if we're in LLDB mode by checking for lldb module
            is_lldb = 'lldb' in sys.modules

            if is_lldb:
                # Extract values from dictionary in Python (safe)
                variable_name = buffer_metadata.get('variable_name', '')
                display_name = buffer_metadata.get('display_name', '')
                width = buffer_metadata.get('width', 0)
                height = buffer_metadata.get('height', 0)
                channels = buffer_metadata.get('channels', 0)
                type_val = buffer_metadata.get('type', 0)
                row_stride = buffer_metadata.get('row_stride', 0)
                pixel_layout = buffer_metadata.get('pixel_layout', 'rgba')
                transpose_buffer = buffer_metadata.get('transpose_buffer', False)

                # Get pointer and size from memoryview using ctypes (safe)
                import ctypes
                mv = buffer_metadata.get('pointer')
                if mv is not None:
                    try:
                        # For memoryview, access the underlying object
                        # The memoryview from LLDB is created from bytes (process.ReadMemory)
                        underlying = mv.obj if hasattr(mv, 'obj') else mv

                        # If underlying object has buffer_info (like array.array), use it
                        if hasattr(underlying, 'buffer_info'):
                            buffer_info = underlying.buffer_info()
                            buffer_ptr = buffer_info[0]
                        else:
                            # For bytes (read-only), we need to create a writable copy
                            # to get a pointer. This is necessary because ctypes.from_buffer
                            # requires writable buffers, and bytes are immutable.
                            # Create a bytearray (writable) from the memoryview
                            bytes_copy = bytearray(mv)
                            # Now we can use from_buffer on the writable bytearray
                            c_array = (ctypes.c_ubyte * len(bytes_copy)).from_buffer(bytes_copy)
                            buffer_ptr = ctypes.addressof(c_array)
                            # Keep reference to prevent garbage collection
                            if not hasattr(self, '_buffer_refs'):
                                self._buffer_refs = []
                            self._buffer_refs.append(bytes_copy)
                            self._buffer_refs.append(c_array)

                        buffer_size = mv.nbytes
                        # Keep reference to prevent garbage collection
                        if not hasattr(self, '_buffer_refs'):
                            self._buffer_refs = []
                        self._buffer_refs.append(mv)
                    except Exception as e:
                        # Fallback: if we can't get pointer safely, skip plotting
                        import logging
                        log = logging.getLogger(__name__)
                        log.warning(f"Could not extract buffer pointer from memoryview: {e}")
                        # Don't remove from cache - this is a transient error, keep buffer
                        # in cache for next attempt. The buffer is still valid.
                        return
                else:
                    # Pointer is None - don't remove from cache if process is running
                    # Only remove if process is stopped (variable doesn't exist)
                    import sys
                    if 'lldb' in sys.modules:
                        import lldb
                        try:
                            debugger = self._bridge.get_lldb_backend()
                            if debugger and debugger.GetSelectedTarget().IsValid():
                                process = debugger.GetSelectedTarget().process
                                if process.IsValid() and process.GetState() == lldb.eStateStopped:
                                    # Process is stopped but pointer is None - variable doesn't exist
                                    if self._window is not None:
                                        self._window._observed_buffers_cache.discard(self._variable)
                                # If process is running or we can't determine state, keep buffer in cache
                        except Exception:
                            # If we can't check process state, keep buffer in cache to be safe
                            pass
                    else:
                        # Non-LLDB mode: remove from cache
                        if self._window is not None:
                            self._window._observed_buffers_cache.discard(self._variable)
                    return

                # Call safe version with individual parameters
                self._lib.oid_plot_buffer_safe(
                    self._native_handler,
                    variable_name.encode('utf-8') if isinstance(variable_name, str) else variable_name,
                    display_name.encode('utf-8') if isinstance(display_name, str) else display_name,
                    buffer_ptr,
                    buffer_size,
                    width,
                    height,
                    channels,
                    type_val,
                    row_stride,
                    pixel_layout.encode('utf-8') if isinstance(pixel_layout, str) else pixel_layout,
                    1 if transpose_buffer else 0)
            else:
                # Non-LLDB mode: use original function with dictionary
                self._lib.oid_plot_buffer(self._native_handler, buffer_metadata)

        except Exception as err:
            import traceback
            import logging
            log = logging.getLogger(__name__)
            log.error("Could not plot variable")
            log.error(err)
            traceback.print_exc()
            # Don't remove from cache on exception - might be transient (process running, etc.)
            # Keep buffer in cache for next stop event. Only remove if it's a permanent error.
            # For now, we keep it in cache to avoid losing track of buffers when continuing quickly.
