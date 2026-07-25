# -*- coding: utf-8 -*-

"""
Code responsible with directly interacting with LLDB
"""

import lldb
import re
import time
import threading

from oidscripts import sysinfo
from oidscripts.typebridge import TypeInspectorInterface
from oidscripts.debuggers.interfaces import BridgeInterface, \
    DebuggerSymbolReference, raise_if_too_large
from oidscripts.debuggers.template_args import TemplateTypeName

instance = None

# A plain symbol name (identifier), as opposed to a dotted/complex expression.
# These are looked up with frame.FindVariable(), which avoids lldb's expression
# JIT and its trouble resolving heavy templated types (e.g. Eigen).
_PLAIN_IDENTIFIER_RE = re.compile(r'^[A-Za-z_]\w*$')


def frame_from_debugger(debugger):
    # type: (lldb.SBDebugger) -> lldb.SBFrame
    """Walk debugger -> process -> thread -> frame, picking the thread with
    a real stop reason. Every step is guarded with `not x` rather than
    `is None`: SB objects are falsy when invalid but are not None."""
    # Local, not the module import above: re-resolves sys.modules at call
    # time. The module import binds once, permanently, on first import, so
    # a caller holding a different `lldb` (oid_resolve_host.py, under test)
    # would otherwise never be seen here.
    import lldb
    if not debugger:
        return None
    target = debugger.GetSelectedTarget()
    if not target:
        return None
    process = target.process
    if not process:
        return None
    for thread in process:
        stop_reason = thread.GetStopReason()
        if stop_reason != lldb.eStopReasonNone and \
                stop_reason != lldb.eStopReasonInvalid:
            return thread.GetSelectedFrame()
    return None


def evaluate_in_frame(frame, expression):
    # type: (lldb.SBFrame, str) -> SymbolWrapper
    """Evaluate `expression` in `frame`: FindVariable fast path for a plain
    identifier (sidesteps lldb's expression JIT, which struggles with heavy
    templated types e.g. Eigen), else EvaluateExpression. Raises
    RuntimeError on failure."""
    if _PLAIN_IDENTIFIER_RE.match(expression):
        variable = frame.FindVariable(expression)
        if variable.IsValid():
            return SymbolWrapper(variable)

    result = frame.EvaluateExpression(expression)
    # A result can be truthy yet invalid, and GetError() need not return an
    # object, so guard both rather than chaining off either; result itself
    # may also be falsy/None outright (e.g. a dead frame).
    error = result.GetError() if result else None
    if not result or not result.IsValid() or \
            (error is not None and error.Fail()):
        message = error.GetCString() if error is not None else None
        raise RuntimeError(
            f'Expression "{expression}" failed: '
            f'{message or "invalid result"}')
    return SymbolWrapper(result)


def _walk_members(symbol, member_name_chain, visited_typenames, type_bridge,
                   record_hit):
    # type: (lldb.SBValue, list, frozenset, TypeInspectorInterface, callable) -> None
    """Recursive descent into `symbol`'s members, calling
    `record_hit(qualified_name, SymbolWrapper)` for each observable one."""
    if symbol.GetTypeName() in visited_typenames:
        return  # this type is already on the path from the root
    # Mark this symbol's type, not the child's, before descending: the
    # latter trips the recursive call's own guard on its first check,
    # stopping traversal after one hop. Copy rather than mutate, so
    # sibling branches don't block each other from descending a type.
    visited_typenames = visited_typenames | {symbol.GetTypeName()}

    for member_idx in range(symbol.GetNumChildren()):
        symbol_member = symbol.GetChildAtIndex(member_idx)
        chain = member_name_chain + [str(symbol_member.name)]
        wrapped = SymbolWrapper(symbol_member)
        if type_bridge.is_symbol_observable(wrapped, symbol_member.name):
            record_hit('.'.join(chain), wrapped)
        else:
            _walk_members(symbol_member, chain, visited_typenames,
                          type_bridge, record_hit)


def observable_symbols(frame, type_bridge):
    # type: (lldb.SBFrame, TypeInspectorInterface) -> list
    """Every observable symbol in `frame`: top-level plus struct/class
    members reached by recursive descent (e.g. `this.image`). Returns
    (qualified_name, SymbolWrapper) pairs, deduped, in discovery order --
    callers shape the pairs into whatever result they need."""
    found = []
    seen = set()

    def emit(qualified_name, wrapped):
        if qualified_name not in seen:
            seen.add(qualified_name)
            found.append((qualified_name, wrapped))

    for symbol in frame.GetVariables(True, True, True, True):
        name = symbol.name
        if not name:
            continue
        wrapped = SymbolWrapper(symbol)
        if type_bridge.is_symbol_observable(wrapped, name):
            emit(name, wrapped)
        member_name_chain = [name] if name != 'this' else []
        _walk_members(symbol, member_name_chain, frozenset(), type_bridge,
                      emit)

    return found


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

    def get_buffer_metadata(self, variable, max_bytes=None):
        # type: (str) -> dict
        process = self._get_process(self.get_lldb_backend())
        thread = self._get_thread(process)
        frame = self._get_frame(thread)

        if not frame:
            # Could not fetch frame from debugger state (None, or an
            # invalid SBFrame that is falsy but not None).
            return None

        # Prefer a direct variable lookup: it is the same robust path
        # get_available_symbols() uses, and it avoids lldb's expression JIT,
        # which can fail to resolve heavy templated types (e.g. Eigen) on
        # some architectures (observed on x86_64). Fall back to expression
        # evaluation so dotted/complex symbol paths still work.
        picked_obj = frame.FindVariable(variable)  # type: lldb.SBValue
        if not picked_obj.IsValid():
            picked_obj = frame.EvaluateExpression(variable)

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
            raise RuntimeError('Invalid null buffer pointer')
        if bufsize == 0:
            raise ValueError('Invalid buffer of zero bytes')
        elif bufsize >= sysinfo.get_available_memory() / 10:
            raise MemoryError('Invalid buffer size larger than available memory')

        raise_if_too_large(bufsize, max_bytes)

        buffer_metadata['variable_name'] = variable

        # ReadMemory returns None on failure (e.g. the address went stale
        # after the frame changed); surface a descriptive error instead of
        # letting memoryview() fail on the None.
        read_error = lldb.SBError()
        buffer_contents = process.ReadMemory(
            buffer_metadata['pointer'], bufsize, read_error)
        if read_error.Fail() or buffer_contents is None:
            raise RuntimeError(
                'Could not read buffer memory for "{}" at address {}'
                ' ({} bytes): {}'.format(
                    variable, buffer_metadata['pointer'], bufsize,
                    read_error.GetCString() or 'unknown error'))
        buffer_metadata['pointer'] = memoryview(buffer_contents)

        return buffer_metadata

    def register_event_handlers(self, event_handler):
        self._event_handler = event_handler

    def get_casted_pointer(self, typename, lldb_object):
        return lldb_object.get_casted_pointer()

    def evaluate_expression(self, expression):
        # The interface contract is to raise RuntimeError on any failure so
        # the declarative engine can treat evaluation errors uniformly. The
        # intentional RuntimeErrors below carry the best message; any other
        # backend exception is normalized to RuntimeError in the fallback.
        try:
            # frame_from_debugger() can return an invalid SBFrame (falsy but
            # not None), so use a truthiness guard rather than "is None".
            frame = frame_from_debugger(self.get_lldb_backend())
            if not frame:
                raise RuntimeError(
                    f'Expression "{expression}" failed: no stopped frame')
            return evaluate_in_frame(frame, expression)
        except RuntimeError:
            raise
        except Exception as error:
            raise RuntimeError(
                f'Expression "{expression}" failed: {error}') from error

    def get_available_symbols(self):
        frame = frame_from_debugger(self.get_lldb_backend())
        if not frame:
            return set()
        # Callers historically get bare names; observable_symbols() returns
        # richer (name, wrapper) pairs so other callers can shape their own.
        return {name for name, _ in observable_symbols(frame, self._type_bridge)}

    def stop_hook(self, *args):
        with self._lock:
            self._event_queue.append('stop')


class SymbolWrapper(DebuggerSymbolReference):
    def __init__(self, symbol):
        self._symbol = symbol  # type: lldb.SBValue
        # A str carrying the type name, plus GDB-style template_argument()
        # backed by the canonical name so the Eigen inspector works under
        # LLDB (whose GetTypeName() is a typedef with no template params).
        sbtype = symbol.GetType()
        canonical = (sbtype.GetCanonicalType().GetName() or None) \
            if sbtype.IsValid() else None
        self.type = TemplateTypeName(symbol.GetTypeName(), canonical)

    def __str__(self):
        return str(self._symbol.GetValue())

    def __int__(self):
        string_value = self._symbol.GetValue()
        try:
            # LLDB may render integers in decimal or hex (e.g. '0x10'); base 0
            # parses both, plus the 0o/0b prefixes. Fall back to the scalar
            # accessor when there is no parseable textual value.
            return int(string_value, 0)
        except (TypeError, ValueError):
            return self._symbol.GetValueAsSigned()

    def __float__(self):
        string_value = self._symbol.GetValue()
        return float(string_value)

    def __getitem__(self, member):
        child = self._symbol.GetChildAtIndex(member) if isinstance(member, int) \
                else self._symbol.GetChildMemberWithName(str(member))

        if not child.IsValid():
            raise KeyError

        return SymbolWrapper(child)

    def get_casted_pointer(self):
        symbol = self._symbol
        if symbol.TypeIsPointerType():
            return symbol.GetValueAsUnsigned()

        # A scalar integer result already *is* the address (e.g. a
        # user-authored JSON expression yielding a uintptr_t, or a numeric
        # literal like 0x...). Read its value directly; AddressOf() would
        # instead return the address of the temporary that holds it.
        if symbol.GetType().GetTypeFlags() & lldb.eTypeIsInteger:
            return symbol.GetValueAsUnsigned()

        return symbol.AddressOf().GetValueAsUnsigned()
