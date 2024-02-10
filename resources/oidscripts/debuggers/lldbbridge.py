# -*- coding: utf-8 -*-

"""
Code responsible with directly interacting with LLDB
"""

import lldb
import time
import threading

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

        # Store debugger from the main thread since it isn't available from an event loop.
        self._debugger = lldb.debugger

        event_loop_thread = threading.Thread(target=self.event_loop)
        event_loop_thread.daemon = True
        event_loop_thread.start()

    def get_lldb_backend(self):
        # type: () -> lldb.SBDebugger
        return self._debugger

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

            member_name_chain_current = member_name_chain.copy()
            member_name_chain_current.append(str(symbol_member.name))

            symbol_wrapper = SymbolWrapper(symbol_member)
            if self._type_bridge.is_symbol_observable(symbol_wrapper,
                                                      symbol_member.name):
                member_name_current = '.'.join(member_name_chain_current)
                output_set.add(member_name_current)
            else:
                if symbol_member.name != symbol_member.GetTypeName():
                    visited_typenames.add(symbol_member.GetTypeName())
                self._get_observable_children_members(symbol_member,
                                                      member_name_chain_current,
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
        child = self._symbol.GetChildAtIndex(member) if isinstance(member, int) \
                else selg._symbol.GetChildMemberWithName(str(member))

        if not child.IsValid():
            raise KeyError

        return SymbolWrapper(child)

    def get_casted_pointer(self):
        if self._symbol.TypeIsPointerType():
            buff_addr = self._symbol.GetValueAsUnsigned()
        else:
            buff_addr = self._symbol.AddressOf().GetValueAsUnsigned()

        return buff_addr
