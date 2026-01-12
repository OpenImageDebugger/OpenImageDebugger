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
        # #region agent log
        import json, threading
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H3','location':'oidwindow.py:27','message':'OpenImageDebuggerWindow_init_start','data':{'thread':threading.current_thread().name},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        self._bridge = bridge
        self._script_path = script_path

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
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H3','location':'oidwindow.py:43','message':'loading_library','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        self._lib = ctypes.cdll.LoadLibrary(
            script_path + '/' + OpenImageDebuggerWindow.__get_library_name())
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H3','location':'oidwindow.py:46','message':'library_loaded','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion

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

        # H19: LLDB-safe version that takes individual parameters
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
        # #region agent log
        import json, threading
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H15','location':'oidwindow.py:115','message':'plot_variable_entry','data':{'thread':threading.current_thread().name,'requested_symbol':str(requested_symbol)[:100]},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion

        if self._bridge is None:
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H15','location':'oidwindow.py:128','message':'plot_variable_bridge_none','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion
            log.info(f"Could not plot symbol {requested_symbol}: Not a debugging session")
            return 0

        try:
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H15','location':'oidwindow.py:135','message':'plot_variable_before_decode','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion

            if not isinstance(requested_symbol, str):
                variable = requested_symbol.decode('utf-8')
            else:
                variable = requested_symbol

            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H15','location':'oidwindow.py:143','message':'plot_variable_before_deferred','data':{'variable':variable[:100]},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion

            plot_callable = DeferredVariablePlotter(variable,
                                                    self._lib,
                                                    self._bridge,
                                                    self._native_handler)

            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H15','location':'oidwindow.py:150','message':'plot_variable_before_queue','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion

            self._bridge.queue_request(plot_callable)

            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H15','location':'oidwindow.py:155','message':'plot_variable_after_queue','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion

            return 1
        except Exception as err:
            # #region agent log
            import traceback
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H15','location':'oidwindow.py:160','message':'plot_variable_exception','data':{'error':str(err),'traceback':traceback.format_exc()},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion
            log.error('Could not plot variable')
            log.error(err)

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
        # #region agent log
        import json
        import time
        log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
        log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "oidwindow.py:216", "message": "set_available_symbols_entry", "data": {"symbols_count": len(observable_symbols)}, "timestamp": int(time.time() * 1000)}) + "\n")
        log_file.close()
        # #endregion

        # Perform two-level sorting:
        #    1. By amount of nodes.
        #    2. By variable name.
        # #region agent log
        log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
        log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "oidwindow.py:225", "message": "before_sorted", "data": {}, "timestamp": int(time.time() * 1000)}) + "\n")
        log_file.close()
        # #endregion
        sorted_observable_symbols = sorted(observable_symbols, key=lambda symbol: (symbol.count('.'), symbol))
        # #region agent log
        log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
        log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "oidwindow.py:228", "message": "after_sorted", "data": {"sorted_count": len(sorted_observable_symbols)}, "timestamp": int(time.time() * 1000)}) + "\n")
        log_file.close()
        # #endregion

        # #region agent log
        log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
        log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "oidwindow.py:231", "message": "before_oid_set_available_symbols_call", "data": {}, "timestamp": int(time.time() * 1000)}) + "\n")
        log_file.close()
        # #endregion
        self._lib.oid_set_available_symbols(
            self._native_handler,
            sorted_observable_symbols)
        # #region agent log
        log_file = open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a')
        log_file.write(json.dumps({"sessionId": "debug-session", "runId": "run1", "hypothesisId": "H18", "location": "oidwindow.py:235", "message": "after_oid_set_available_symbols_call", "data": {}, "timestamp": int(time.time() * 1000)}) + "\n")
        log_file.close()
        # #endregion

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
        # H18: In LLDB mode, oid_get_observed_buffers returns Py_None, which may
        # not convert correctly via ctypes. Try to call it, and if it returns
        # None or fails, return an empty list instead.
        try:
            result = self._lib.oid_get_observed_buffers(self._native_handler)
            # If result is None or not a list, return empty list
            if result is None or not isinstance(result, list):
                return []
            return result
        except Exception:
            # If call fails, return empty list
            return []

    def initialize_window(self):
        # Initialize OID lib
        # #region agent log
        import json, threading, traceback
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H4','location':'oidwindow.py:202','message':'calling_oid_initialize','data':{'thread':threading.current_thread().name},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        try:
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H4','location':'oidwindow.py:209','message':'before_oid_initialize_call','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion
            self._native_handler = self._lib.oid_initialize(
                self._plot_variable_c_callback,
                {'oid_path': self._script_path})
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H4','location':'oidwindow.py:215','message':'oid_initialize_returned','data':{'handler_is_none':self._native_handler is None},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion
        except Exception as e:
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H4','location':'oidwindow.py:220','message':'oid_initialize_exception','data':{'error':str(e),'traceback':traceback.format_exc()},'timestamp':int(__import__('time').time()*1000)})+'\n')
            except: pass
            # #endregion
            raise

        # Launch UI
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H4','location':'oidwindow.py:228','message':'before_oid_exec','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion
        self._lib.oid_exec(self._native_handler)
        # #region agent log
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H4','location':'oidwindow.py:232','message':'after_oid_exec','data':{},'timestamp':int(__import__('time').time()*1000)})+'\n')
        except: pass
        # #endregion

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
        # #region agent log
        import json, threading, time
        try:
            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:371','message':'DeferredVariablePlotter_call_entry','data':{'thread':threading.current_thread().name,'variable':self._variable},'timestamp':int(time.time()*1000)})+'\n')
        except: pass
        # #endregion
        try:
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:375','message':'before_get_buffer_metadata','data':{'variable':self._variable},'timestamp':int(time.time()*1000)})+'\n')
            except: pass
            # #endregion
            buffer_metadata = self._bridge.get_buffer_metadata(self._variable)
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:380','message':'after_get_buffer_metadata','data':{'buffer_metadata_is_none':buffer_metadata is None},'timestamp':int(time.time()*1000)})+'\n')
            except: pass
            # #endregion

            if buffer_metadata is None:
                return

            # H19: In LLDB mode, use the safe version that doesn't require
            # Python C API calls to parse the dictionary. Extract values in
            # Python and pass as individual arguments.
            import sys
            # Check if we're in LLDB mode by checking for lldb module
            is_lldb = 'lldb' in sys.modules
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:390','message':'lldb_mode_check','data':{'is_lldb':is_lldb},'timestamp':int(time.time()*1000)})+'\n')
            except: pass
            # #endregion
            
            if is_lldb:
                # #region agent log
                try:
                    with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                        f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:395','message':'entering_lldb_branch','data':{},'timestamp':int(time.time()*1000)})+'\n')
                except: pass
                # #endregion
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
                # #region agent log
                try:
                    with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                        f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:407','message':'after_extract_metadata','data':{'variable_name':str(variable_name)[:50],'width':width,'height':height,'channels':channels},'timestamp':int(time.time()*1000)})+'\n')
                except: pass
                # #endregion
                
                # Get pointer and size from memoryview using ctypes (safe)
                import ctypes
                mv = buffer_metadata.get('pointer')
                # #region agent log
                try:
                    with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                        f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:412','message':'before_get_pointer','data':{'mv_is_none':mv is None},'timestamp':int(time.time()*1000)})+'\n')
                except: pass
                # #endregion
                if mv is not None:
                    try:
                        # #region agent log
                        try:
                            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:416','message':'before_get_pointer_from_mv','data':{},'timestamp':int(time.time()*1000)})+'\n')
                        except: pass
                        # #endregion
                        import ctypes
                        # For memoryview, access the underlying object
                        # The memoryview from LLDB is created from bytes (process.ReadMemory)
                        underlying = mv.obj if hasattr(mv, 'obj') else mv
                        # #region agent log
                        try:
                            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:422','message':'got_underlying_object','data':{'underlying_type':str(type(underlying)),'has_buffer_info':hasattr(underlying,'buffer_info')},'timestamp':int(time.time()*1000)})+'\n')
                        except: pass
                        # #endregion
                        
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
                        # #region agent log
                        try:
                            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:444','message':'after_pointer_extraction','data':{'buffer_ptr':buffer_ptr is not None,'buffer_size':buffer_size},'timestamp':int(time.time()*1000)})+'\n')
                        except: pass
                        # #endregion
                        # Keep reference to prevent garbage collection
                        if not hasattr(self, '_buffer_refs'):
                            self._buffer_refs = []
                        self._buffer_refs.append(mv)
                    except Exception as e:
                        # Fallback: if we can't get pointer safely, skip plotting
                        import logging
                        log = logging.getLogger(__name__)
                        log.warning(f"Could not extract buffer pointer from memoryview: {e}")
                        # #region agent log
                        try:
                            with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                                f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:456','message':'pointer_extraction_exception','data':{'error':str(e)},'timestamp':int(time.time()*1000)})+'\n')
                        except: pass
                        # #endregion
                        return
                else:
                    # #region agent log
                    try:
                        with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                            f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:437','message':'mv_is_none_returning','data':{},'timestamp':int(time.time()*1000)})+'\n')
                    except: pass
                    # #endregion
                    return
                
                # #region agent log
                try:
                    with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                        f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:420','message':'before_oid_plot_buffer_safe','data':{'variable_name':variable_name,'buffer_ptr':buffer_ptr is not None,'buffer_size':buffer_size},'timestamp':int(time.time()*1000)})+'\n')
                except: pass
                # #endregion
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
                # #region agent log
                try:
                    with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                        f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:435','message':'after_oid_plot_buffer_safe','data':{},'timestamp':int(time.time()*1000)})+'\n')
                except: pass
                # #endregion
            else:
                # Non-LLDB mode: use original function with dictionary
                self._lib.oid_plot_buffer(
                    self._native_handler,
                    buffer_metadata)

        except Exception as err:
            import traceback, logging
            log = logging.getLogger(__name__)
            log.error("Could not plot variable")
            log.error(err)
            traceback.print_exc()
            # #region agent log
            try:
                with open('/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log', 'a') as f:
                    f.write(json.dumps({'sessionId':'debug-session','runId':'run1','hypothesisId':'H19','location':'oidwindow.py:450','message':'DeferredVariablePlotter_exception','data':{'error':str(err)},'timestamp':int(time.time()*1000)})+'\n')
            except: pass
            # #endregion
