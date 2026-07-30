# -*- coding: utf-8 -*-

"""Debugger-facing half of the JSON resolver: the bridge adapter the
declarative engine needs, plus selection of whichever debugger module is
present in the host interpreter.

Both debuggers are wired in: LldbHost binds an lldb frame, GdbHost the
gdb-selected frame, selected by current_host() with no change to
oid_resolve.py."""

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


class GdbAdapter(object):
    """The whole bridge contract the engine exercises, over gdb idioms:
    evaluate an expression, and cast a symbol to its buffer address."""

    def evaluate_expression(self, expression):
        import gdb
        try:
            return gdb.parse_and_eval(expression)
        except Exception as error:
            raise RuntimeError(
                'Expression "%s" failed: %s' % (expression, error)) from error

    def get_casted_pointer(self, typename, obj):
        import gdb
        return obj.cast(gdb.lookup_type(typename).pointer())


def _peel_gdb_type(gdb, node_type):
    """Typedefs and references peel before the struct check: a `Wrapper&`
    local (any reference parameter) is field-navigated with '.' exactly
    like a `Wrapper`, and the declarative engine already treats `T&` as
    not-a-pointer. Pointers stay pointers -- their members would need a
    dereference the emitted names do not spell."""
    peeled = node_type
    strip = getattr(peeled, 'strip_typedefs', None)
    if strip is not None:
        peeled = strip()
    ref_codes = (gdb.TYPE_CODE_REF,
                 getattr(gdb, 'TYPE_CODE_RVALUE_REF', None))
    if peeled.code in ref_codes:
        peeled = peeled.target()
        strip = getattr(peeled, 'strip_typedefs', None)
        if strip is not None:
            peeled = strip()
    return peeled


def _walk_gdb_members(gdb, bridge, node, parent_name, emit):
    """Emit `node`'s observable struct/class members, recursively.

    An empty `parent_name` makes members surface BARE -- used for `this`,
    whose members are directly evaluable in the frame through the implicit
    this. That matches the names lldbbridge emits, so a client can feed any
    list_observable() item straight back into resolve() under either
    debugger."""
    node_type = _peel_gdb_type(gdb, node.type)
    if gdb.TYPE_CODE_STRUCT != node_type.code:
        return
    for field in node_type.fields():
        field_name = getattr(field, 'name', None)
        if not field_name or getattr(field, 'is_base_class', False):
            # Anonymous aggregates and base-class subobjects contribute no
            # path segment: C++ addresses their members directly on the
            # containing object, so an invented segment ('parent.None.x',
            # 'parent.Base.x') would never evaluate back through
            # resolve(). Recurse as if their members were direct members.
            _walk_gdb_members(gdb, bridge, field, parent_name, emit)
            continue
        qualified_name = ('%s.%s' % (parent_name, field_name)
                          if parent_name else field_name)
        if bridge.is_symbol_observable(field, qualified_name):
            emit(qualified_name, field.type)
        else:
            _walk_gdb_members(gdb, bridge, field, qualified_name, emit)


def _gdb_scope_names(block):
    """Every named local/argument across the block chain -- the names an
    unqualified C++ expression would resolve BEFORE an implicit
    this-member."""
    names = set()
    while block is not None:
        for symbol in block:
            if (getattr(symbol, 'is_argument', False)
                    or getattr(symbol, 'is_variable', False)):
                name = getattr(symbol, 'name', None)
                if name:
                    names.add(name)
        block = getattr(block, 'superblock', None)
    return names


def _expand_gdb_symbol(gdb, bridge, symbol, emit, shadowed=frozenset()):
    """One in-scope symbol: kept when its own type is observable, expanded
    into its observable members otherwise (`this` dereferenced first, its
    members emitted bare -- except members whose bare name a local or
    argument in `shadowed` would capture: C++ resolves the unqualified
    name to the local, so listing the member would resolve the wrong
    object)."""
    name = symbol.name
    if bridge.is_symbol_observable(symbol, name):
        emit(name, symbol.type)
    elif name == 'this':
        try:
            this_value = gdb.parse_and_eval('this').dereference()
        except Exception:                          # noqa: BLE001
            # `this` is in scope but not evaluable right now (optimized
            # out, frame prologue, dead frame). Skip its expansion rather
            # than aborting the whole listing -- the caller turns any
            # escaped exception into a full-page error payload, which
            # would hide every other observable symbol.
            return

        def emit_unshadowed(member_name, member_type):
            if member_name.split('.', 1)[0] not in shadowed:
                emit(member_name, member_type)

        _walk_gdb_members(gdb, bridge, this_value, '', emit_unshadowed)
    else:
        _walk_gdb_members(gdb, bridge, symbol, name, emit)


class GdbHost(object):
    """Binds oid_resolve's host contract to gdb's selected frame.

    Unlike LldbHost, no frame is captured at construction: gdb's
    parse_and_eval and selected_frame() always act on the current selection,
    so a host held across stops stays correct by construction.
    """

    def __init__(self):
        self._adapter = GdbAdapter()

    def buffer_metadata(self, name):
        # Evaluate through the adapter, not gdb.parse_and_eval directly:
        # resolve() stringifies whatever escapes, and the adapter is the
        # error-normalization contract ('Expression "..." failed: ...')
        # shared with the engine's own expression evaluation.
        picked = self._adapter.evaluate_expression(name)
        return _type_bridge().get_buffer_metadata(name, picked, self._adapter)

    def observable_symbols(self):
        """Every observable symbol in the current frame, including buffers
        held as struct/class members (`holder.image`; members of `this`
        surface bare, e.g. `image`, exactly as lldbbridge names them) --
        an innermost-block-first walk of the selected frame's blocks. Each
        in-scope local/argument is kept when its own type is a registered
        plottable buffer type; a struct/class symbol that is not itself
        observable is expanded into its observable members instead.
        Deduplicated by qualified name (an inner shadowing wins), as
        {'name','type'} pairs -- the same shape LldbHost returns, so the
        paging in oid_resolve.list_observable slices it unchanged."""
        import gdb
        try:
            block = gdb.selected_frame().block()
        except Exception as error:                 # noqa: BLE001
            # Before `run`, or after the inferior exits, gdb raises here.
            # Name the routine condition the way the lldb path does
            # instead of letting gdb's internal message surface as an
            # opaque endpoint error.
            raise RuntimeError(
                'gdb is available in this interpreter, but no stopped '
                'frame is available') from error
        bridge = _type_bridge()
        shadowed = _gdb_scope_names(block)
        seen = set()
        out = []
        processed = set()

        def emit(qualified_name, type_obj):
            if qualified_name not in seen:
                seen.add(qualified_name)
                out.append({'name': qualified_name, 'type': str(type_obj)})

        while block is not None:
            for symbol in block:
                if not (getattr(symbol, 'is_argument', False)
                        or getattr(symbol, 'is_variable', False)):
                    continue
                # Unnamed symbols are skipped like lldbbridge does: a null
                # name (or a None-parented member path) could never be fed
                # back through resolve().
                name = getattr(symbol, 'name', None)
                if not name or name in processed:
                    continue
                processed.add(name)
                _expand_gdb_symbol(gdb, bridge, symbol, emit, shadowed)
            block = getattr(block, 'superblock', None)
        return out


def _frame_from_lldb_debugger(lldb):
    """Walk from lldb.debugger down to the selected frame, rather than
    trusting lldb.frame -- a convenience global that only exists in some
    lldb script contexts. Delegates the actual walk to lldbbridge's shared
    helper once lldb.debugger is extracted."""
    from oidscripts.debuggers.lldbbridge import frame_from_debugger
    return frame_from_debugger(getattr(lldb, 'debugger', None))


# Every attribute the gdb path reads WITHOUT a guard must be listed here,
# or a partial module passes selection and crashes mid-walk instead
# (TYPE_CODE_RVALUE_REF stays out: _peel_gdb_type reads it via getattr).
_GDB_HOST_API = ('selected_frame', 'parse_and_eval', 'lookup_type',
                 'TYPE_CODE_STRUCT', 'TYPE_CODE_REF')


def _gdb_serves_host(gdb):
    """A module named gdb only hosts this interpreter when it carries the
    API this module drives (GdbAdapter, GdbHost, and the member walk). An
    importable stub -- a leftover test fake, a foreign package shadowing
    the name -- must fall through to the terminal no-debugger error, the
    same way an importable-but-inert lldb falls through to the gdb probe,
    instead of yielding a host that fails later with AttributeError."""
    return all(hasattr(gdb, attr) for attr in _GDB_HOST_API)


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
    if gdb is not None and _gdb_serves_host(gdb):
        return GdbHost()

    raise RuntimeError(
        'no supported debugger module is available in this interpreter')
