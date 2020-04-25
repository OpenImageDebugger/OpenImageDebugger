# -*- coding: utf-8 -*-

"""
Code responsible with directly interacting with LLDB
"""

import lldb
import time
import threading
import sys

from oidscripts import sysinfo
from oidscripts.typebridge import TypeInspectorInterface
from oidscripts.debuggers.interfaces import BridgeInterface, \
    DebuggerSymbolReference

instance = None


class LldbBridge(BridgeInterface):
    """
    LLDB Bridge class, exposing the common expected interface for the OpenImageDebugger
    to access the required buffer data and interact with the debugger.
    """

    def __init__(self, type_bridge):
        # type: (TypeInspectorInterface) -> None
        global instance
        instance = self
        self._type_bridge = type_bridge
        self._pending_requests = []
        self._lock = threading.Lock()
        self._event_queue = []
        self._event_handler = None
        self._last_thread_id = 0
        self._last_frame_idx = 0
        event_loop_thread = threading.Thread(target=self.event_loop)
        event_loop_thread.daemon = True
        event_loop_thread.start()

    def get_lldb_backend(self):
        # type: () -> lldb.SBDebugger
        return lldb.debugger

    def get_backend_name(self):
        return 'lldb'

    def _check_frame_modification(self):
        process = self._get_process(self.get_lldb_backend())
        if process.is_stopped:
            thread = self._get_thread(process)
            frame = self._get_frame(thread)

            thread_id = thread.id if thread is not None else 0
            frame_idx = frame.idx if thread is not None else 0

            frame_was_updated = thread_id != self._last_thread_id or \
                                frame_idx != self._last_frame_idx

            self._last_thread_id = thread_id
            self._last_frame_idx = frame_idx

            if frame_was_updated:
                with self._lock:
                    self._event_queue.append('stop')

    def event_loop(self):
        while True:
            self._check_frame_modification()

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
                if event == 'stop' and self._event_handler is not None:
                    self._event_handler.stop_handler()

            while requests_to_process:
                callback = requests_to_process.pop(0)
                callback()

            time.sleep(0.1)

    def queue_request(self, callable_request):
        # type: (Callable[[None],None]) -> None
        with self._lock:
            self._pending_requests.append(callable_request)

    def _get_process(self, debugger):
        # type: (lldb.SBDebugger) -> lldb.SBProcess
        return debugger.GetSelectedTarget().process

    def _get_thread(self, process):
        # type: (lldb.SBProcess) -> lldb.SBThread
        for t in process:
            if t.GetStopReason() != lldb.eStopReasonNone and \
                    t.GetStopReason() != lldb.eStopReasonInvalid:
                return t
        return None

    def _get_frame(self, thread):
        # type: (lldb.SBThread) -> lldb.SBFrame
        if not thread:
            return None
        return thread.GetSelectedFrame()

    def get_buffer_metadata(self, variable):
        # type: (str) -> dict
        process = self._get_process(self.get_lldb_backend())
        thread = self._get_thread(process)
        frame = self._get_frame(thread)

        if frame is None:
            # Could not fetch frame from debugger state
            return None

        picked_obj = frame.EvaluateExpression(variable)  # type: lldb.SBValue

        buffer_metadata = self._type_bridge.get_buffer_metadata(
            variable, SymbolWrapper(picked_obj), self)

        if buffer_metadata is None:
            # Invalid symbol for current frame
            return None

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
        elif bufsize >= sysinfo.get_available_memory() / 10:
            raise Exception('Invalid buffer size larger than available memory')

        buffer_metadata['variable_name'] = variable
        buffer_metadata['pointer'] = memoryview(process.ReadMemory(
            buffer_metadata['pointer'], bufsize, lldb.SBError()))

        return buffer_metadata

    def register_event_handlers(self, event_handler):
        self._event_handler = event_handler
        pass

    def get_casted_pointer(self, typename, lldb_object):
        return lldb_object.get_casted_pointer()

    def _get_observable_children_members(self, symbol, member_name_chain,
                                         output_set, visited_typenames=set()):
        # type: (lldb.SBValue, list[str], set, set) -> None
        if symbol.GetTypeName() in visited_typenames:
            # Prevent endless recursion in cyclic data types
            return

        for member_idx in range(symbol.GetNumChildren()):
            symbol_member = symbol.GetChildAtIndex(
                member_idx)  # type: lldb.SBValue
            symbol_wrapper = SymbolWrapper(symbol_member)
            if self._type_bridge.is_symbol_observable(symbol_wrapper,
                                                      symbol_member.name):
                member_name_chain.append(symbol_member.name)
                output_set.add('.'.join(member_name_chain))
            else:
                if symbol_member.name != symbol_member.GetTypeName():
                    member_name_chain.append(symbol_member.name)
                    visited_typenames.add(symbol_member.GetTypeName())
                self._get_observable_children_members(symbol_member,
                                                      member_name_chain,
                                                      output_set,
                                                      visited_typenames)

    def get_available_symbols(self):
        frame = self._get_frame(
            self._get_thread(self._get_process(self.get_lldb_backend())))
        if not frame:
            return set()

        available_symbols = set()

        for symbol in frame.GetVariables(True, True, True, True):
            symbol_wrapper = SymbolWrapper(symbol)
            if self._type_bridge.is_symbol_observable(symbol_wrapper,
                                                      symbol.name):
                available_symbols.add(symbol.name)
            # Check for members of current field, if it is a class
            member_name_chain = [symbol.name] if symbol.name != 'this' else []
            self._get_observable_children_members(symbol, member_name_chain,
                                                  available_symbols)

        return available_symbols

    def stop_hook(self, *args):
        with self._lock:
            self._event_queue.append('stop')


class SymbolWrapper(DebuggerSymbolReference):
    def __init__(self, symbol):
        self._symbol = symbol  # type: lldb.SBValue
        self.type = symbol.GetTypeName()  # type: str

    def __str__(self):
        return str(self._symbol.GetValue())

    def __int__(self):
        string_value = self._symbol.GetValue()
        return int(string_value)

    def __float__(self):
        string_value = self._symbol.GetValue()
        return float(string_value)

    def __getitem__(self, member):
        num_children = self._symbol.GetNumChildren()
        if isinstance(member, int):
            invalid_member_requested = member > num_children
            get_symbol_child = self._symbol.GetChildAtIndex
        elif isinstance(member, str):
            invalid_member_requested = self._symbol.GetIndexOfChildWithName(
                member) > num_children
            get_symbol_child = self._symbol.GetChildMemberWithName

        if invalid_member_requested:
            raise KeyError

        symbol_child = get_symbol_child(member)
        return SymbolWrapper(symbol_child)

    def get_casted_pointer(self):
        if self._symbol.TypeIsPointerType():
            buff_addr = self._symbol.GetValueAsUnsigned()
        else:
            buff_addr = self._symbol.AddressOf().GetValueAsUnsigned()

        return buff_addr
