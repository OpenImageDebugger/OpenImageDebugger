# -*- coding: utf-8 -*-

"""Debugger-facing half of the JSON resolver: the bridge adapter the
declarative engine needs, plus selection of whichever debugger module is
present in the host interpreter.

Only lldb is wired in today. gdb is supported elsewhere in this repo
(debuggers/gdbbridge.py), but routing this entry point through it is
unproven. A GdbHost slots in beside LldbHost, selected the same way from
current_host(), with no change to oid_resolve.py."""

from oidscripts.typebridge import TypeBridge

_BRIDGE = None


def _type_bridge():
    global _BRIDGE
    if _BRIDGE is None:
        _BRIDGE = TypeBridge()
    return _BRIDGE


class LldbAdapter(object):
    """The whole bridge contract the engine exercises: evaluate an
    expression, and cast a symbol to its buffer address."""

    def __init__(self, frame):
        self._frame = frame

    def evaluate_expression(self, expression):
        # Delegates to lldbbridge's shared evaluator: the fast-path lookup,
        # the error guards, and the RuntimeError-on-failure contract all
        # live there now.
        from oidscripts.debuggers.lldbbridge import evaluate_in_frame
        return evaluate_in_frame(self._frame, expression)

    def get_casted_pointer(self, typename, obj):  # NOSONAR
        """Address of the buffer `obj` points at.

        `typename` is the cast target the bridge contract passes; it is
        ignored, since an lldb value already carries its own type.
        """
        return obj.get_casted_pointer()


class LldbHost(object):
    """Binds oid_resolve's host contract to one already-selected lldb frame.

    The frame is captured at construction and never re-derived, so a host
    held across debugger stops queries a stale frame. current_host() builds
    a fresh one per call, so only explicit-host callers need care.
    """

    def __init__(self, frame):
        self._frame = frame
        self._adapter = LldbAdapter(frame)

    def buffer_metadata(self, name):
        from oidscripts.debuggers.lldbbridge import SymbolWrapper
        symbol = self._frame.FindVariable(name)
        if not symbol.IsValid():
            symbol = self._frame.EvaluateExpression(name)
        return _type_bridge().get_buffer_metadata(
            name, SymbolWrapper(symbol), self._adapter)

    def observable_symbols(self):
        """Every observable symbol in the current frame, including buffers
        held as struct/class members (e.g. `this.image`)."""
        from oidscripts.debuggers.lldbbridge import (
            observable_symbols as shared_observable_symbols,
        )
        pairs = shared_observable_symbols(self._frame, _type_bridge())
        return [{'name': name, 'type': str(wrapped.type)}
                for name, wrapped in pairs]


def _frame_from_lldb_debugger(lldb):
    """Walk from lldb.debugger down to the selected frame, rather than
    trusting lldb.frame -- a convenience global that only exists in some
    lldb script contexts. Delegates the actual walk to lldbbridge's shared
    helper once lldb.debugger is extracted."""
    from oidscripts.debuggers.lldbbridge import frame_from_debugger
    return frame_from_debugger(getattr(lldb, 'debugger', None))


def current_host():
    """The host for whichever debugger this interpreter is embedded in."""
    try:
        import lldb
    except ImportError:
        lldb = None

    if lldb is not None:
        # lldb.frame is a convenience global some lldb script contexts
        # populate; prefer it, but it can be absent or a stale-but-present
        # (falsy) SBFrame, so fall back to the explicit walk lldbbridge.py
        # itself always uses.
        frame = getattr(lldb, 'frame', None)
        if not frame:
            frame = _frame_from_lldb_debugger(lldb)
        if frame:
            return LldbHost(frame)
        # `import lldb` succeeding only proves the package is importable --
        # not that lldb is actually hosting this interpreter. On a machine
        # with both debuggers installed, running under gdb, lldb is
        # importable but inert, and its session globals are absent/falsy.
        # Only commit to the lldb-specific error below when some real
        # session marker shows lldb genuinely is hosting this interpreter
        # (a truthy lldb.debugger here; a truthy lldb.frame would already
        # have returned above) -- otherwise fall through to the gdb probe
        # so a merely-importable-but-inert lldb does not mask a real gdb.
        if getattr(lldb, 'debugger', None):
            # lldb is loaded but there is no stopped frame to inspect (e.g.
            # the inferior is running or hasn't been launched yet). That is
            # a routine, distinct condition from "no debugger at all" --
            # say so instead of falling through to the gdb probe and
            # ending on the misleading "no supported debugger" message
            # below.
            raise RuntimeError(
                'lldb is available in this interpreter, but no stopped '
                'frame is available')

    try:
        import gdb
    except ImportError:
        gdb = None
    if gdb is not None:
        # gdb is a real, supported backend in this repo (see the module
        # docstring) -- it just is not served from this entry point yet.
        # Say so plainly instead of claiming no debugger is available.
        raise RuntimeError(
            'gdb is available in this interpreter, but this entry point '
            'does not serve it yet')

    raise RuntimeError(
        'no supported debugger module is available in this interpreter')
