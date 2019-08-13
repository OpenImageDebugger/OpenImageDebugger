# -*- coding: utf-8 -*-

"""
Code responsible with directly interacting with LLDB
"""

import lldb
import time
import threading

from giwscripts import sysinfo
from giwscripts.debuggers.interfaces import BridgeInterface

instance = None

class LldbBridge(BridgeInterface):
    """
    LLDB Bridge class, exposing the common expected interface for the ImageWatch
    to access the required buffer data and interact with the debugger.
    """
    def __init__(self, type_bridge):
        global instance
        instance = self

        self._type_bridge = type_bridge
        self._pending_requests = []
        self._lock = threading.Lock()
        self._event_queue = []
        self._event_handler = None
        event_loop_thread = threading.Thread(target=self.event_loop)
        event_loop_thread.daemon = True
        event_loop_thread.start()

    def event_loop(self):
        while True:
            requests_to_process = []
            with self._lock:
                requests_to_process = self._pending_requests
                self._pending_requests = []

            pending_events = []
            with self._lock:
                pending_events = self._event_queue
                self._event_queue = []

            while pending_events:
                event = pending_events.pop(0)
                if event=='stop':
                    self._event_handler.stop_handler(event)

            while requests_to_process:
                callback = requests_to_process.pop(0)
                callback()

            time.sleep(0.1)

    def queue_request(self, callable_request):
        with self._lock:
            self._pending_requests.append(callable_request)

    def get_frame(self):
        thread = self.get_thread(lldb.debugger)
        if not thread:
            return None
        # TODO find out current frame
        curr_frame = 0
        return thread.GetFrameAtIndex(curr_frame)

    def get_thread(self, debugger):
        for t in self.get_process(debugger):
            if t.GetStopReason() != lldb.eStopReasonNone and \
               t.GetStopReason() != lldb.eStopReasonInvalid:
                return t

        return None

    def get_process(self, debugger):
        return debugger.GetSelectedTarget().process

    def get_buffer_metadata(self, variable):
        thread = self.get_thread(lldb.debugger)
        # TODO determine frame index
        frame = thread.GetFrameAtIndex(0)
        process = thread.GetProcess()
        picked_obj = frame.FindVariable(variable)

        buffer_metadata = self._type_bridge.get_buffer_metadata(
            variable, SymbolWrapper(picked_obj), self)

        bufsize = sysinfo.get_buffer_size(
            buffer_metadata['height'],
            buffer_metadata['channels'],
            buffer_metadata['type'],
            buffer_metadata['row_stride']
        )

        # Check if buffer is initialized
        if buffer_metadata['pointer'] == 0x0:
            raise Exception('Invalid null buffer pointer')
        if bufsize == 0:
            raise Exception('Invalid buffer of zero bytes')
        # TODO implement for macos
        """
        elif bufsize >= sysinfo.get_memory_usage()['free'] / 10:
            raise Exception('Invalid buffer size larger than available memory')
        """

        # TODO Check if buffer is valid. If it isn't, this function will throw an
        # exception

        buffer_metadata['variable_name'] = variable
        buffer_metadata['pointer'] = buffer(process.ReadMemory(\
                buffer_metadata['pointer'], bufsize, lldb.SBError()))

        return buffer_metadata

    def register_event_handlers(self, event_handler):
        self._event_handler = event_handler
        pass

    def get_casted_pointer(self, typename, lldb_object):
        return lldb_object.get_casted_pointer()

    def get_available_symbols(self):
        frame = self.get_frame()
        if not frame:
            return set()

        available_symbols = set()

        # TODO recurse fields
        for symbol in frame.GetVariables(True, True, True, True):
            symbol_wrapper = SymbolWrapper(symbol)
            if self._type_bridge.is_symbol_observable(symbol_wrapper, symbol.name):
                available_symbols.add(symbol.name)

        return available_symbols

    def stop_hook(self, debugger, command, result, dict):
        with self._lock:
            self._event_queue.append('stop')

class SBDataWrapper:
    def __init__(self, symbol, typename):
        self._symbol = symbol
        self.type = typename

    def __str__(self):
        return str(self._symbol.GetString())

    def __int__(self):
        error = lldb.SBError()
        return self._symbol.GetUnsignedInt32(error, 0)

class SymbolWrapper:
    def __init__(self, symbol):
        self._symbol = symbol
        self.type = symbol.GetTypeName()

    def __str__(self):
        return str(self._symbol.GetValue())

    def __int__(self):
        string_value = self._symbol.GetValue()
        return int(string_value)

    def __getitem__(self, member):
        if isinstance(member, int):
            array_value = self._symbol.GetPointeeData(member)
            return SBDataWrapper(array_value, self._symbol.GetTypeName())

        symbol_member = self._symbol.GetChildMemberWithName(member)
        if self._symbol.GetIndexOfChildWithName(member) > self._symbol.GetNumChildren():
            raise KeyError

        if isinstance(symbol_member, lldb.SBValue):
            return SymbolWrapper(symbol_member)
        elif isinstance(symbol_member, lldb.SBData):
            return SBDataWrapper(symbol_member)
        else:
            raise 'Unknown symbol type'

    def get_casted_pointer(self):
        if self._symbol.TypeIsPointerType():
            buff_addr = self._symbol.GetValueAsUnsigned()
        else:
            buff_addr = self._symbol.AddressOf().GetValueAsUnsigned()

        return buff_addr
