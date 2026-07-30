"""The debugger-facing half: bridge adapter plus host selection."""

import sys
import types

import pytest
from conftest import FakeFrame, fake_lldb_module
from oidscripts import oid_resolve_host
from oidscripts.debuggers import lldbbridge

# conftest.py installs the one shared `lldb` stub every test in this suite
# sees (see the comment there for why it must stay the sole owner); nothing
# module-level is needed here.


def test_current_host_without_a_debugger_raises_a_named_error(monkeypatch):
    monkeypatch.delitem(sys.modules, 'lldb', raising=False)
    monkeypatch.delitem(sys.modules, 'gdb', raising=False)
    with pytest.raises(RuntimeError) as excinfo:
        oid_resolve_host.current_host()
    assert 'no supported debugger' in str(excinfo.value).lower()


def test_current_host_with_gdb_present_returns_a_gdb_host(monkeypatch):
    # gdb is a real, supported backend, and this entry point serves it (see
    # the module docstring). Routing requires the module to carry gdb's
    # actual Python API, not merely the name -- _install_fake_gdb (defined
    # with the gdb tests below) provides the surface GdbHost drives.
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(_FakeGdbBlock([])), {})
    host = oid_resolve_host.current_host()
    assert isinstance(host, oid_resolve_host.GdbHost)


def test_current_host_falls_back_to_walking_lldb_debugger_when_frame_attribute_is_absent(monkeypatch):
    monkeypatch.delitem(sys.modules, 'gdb', raising=False)
    expected_frame = FakeFrame()
    # No `.frame` attribute at all: matches interpreters where the
    # convenience global was never populated.
    fake_lldb = fake_lldb_module(selected_frame=expected_frame)
    monkeypatch.setitem(sys.modules, 'lldb', fake_lldb)

    host = oid_resolve_host.current_host()

    assert isinstance(host, oid_resolve_host.LldbHost)
    assert host._frame is expected_frame


def test_current_host_falls_back_when_lldb_frame_is_present_but_falsy(monkeypatch):
    # LLDB SB objects are falsy when invalid but are not None -- a stale,
    # present-but-falsy `lldb.frame` must not be trusted as-is.
    class _StaleFrame:
        def __bool__(self):
            return False

    monkeypatch.delitem(sys.modules, 'gdb', raising=False)
    expected_frame = FakeFrame()
    fake_lldb = fake_lldb_module(
        frame_attr=_StaleFrame(), selected_frame=expected_frame)
    monkeypatch.setitem(sys.modules, 'lldb', fake_lldb)

    host = oid_resolve_host.current_host()

    assert isinstance(host, oid_resolve_host.LldbHost)
    assert host._frame is expected_frame


def test_current_host_with_importable_but_inert_lldb_prefers_gdb(monkeypatch):
    # `import lldb` can succeed merely because the package is installed
    # and importable, without lldb actually hosting this interpreter --
    # e.g. running under gdb on a machine with both debuggers present. No
    # session marker (neither lldb.frame nor lldb.debugger) is set in that
    # case; the probe must not claim lldb is active and must fall through
    # to the real gdb underneath, rather than raising the lldb-specific
    # "no stopped frame" error and masking gdb entirely.
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(_FakeGdbBlock([])), {})
    inert_lldb = types.ModuleType('lldb')
    monkeypatch.setitem(sys.modules, 'lldb', inert_lldb)

    host = oid_resolve_host.current_host()

    assert isinstance(host, oid_resolve_host.GdbHost)


def test_current_host_with_lldb_present_but_no_stopped_frame_names_lldb_not_no_debugger(monkeypatch):
    # lldb is loaded but there is no stopped frame (inferior running, or
    # never launched) -- a routine condition, distinct from "no debugger at
    # all". Falling through to the gdb probe and beyond would end on the
    # misleading "no supported debugger" message; this must name lldb
    # instead, the same defect class already fixed for gdb above.
    monkeypatch.delitem(sys.modules, 'gdb', raising=False)
    # No frame_attr and no selected_frame: the walk finds no stopped thread.
    fake_lldb = fake_lldb_module()
    monkeypatch.setitem(sys.modules, 'lldb', fake_lldb)

    with pytest.raises(RuntimeError) as excinfo:
        oid_resolve_host.current_host()

    message = str(excinfo.value).lower()
    assert 'lldb' in message
    assert 'no stopped frame' in message
    assert 'no supported debugger' not in message


def test_adapter_exposes_the_two_bridge_methods():
    # The declarative engine calls exactly these two on the bridge.
    for method in ('evaluate_expression', 'get_casted_pointer'):
        assert callable(getattr(oid_resolve_host.LldbAdapter, method))


# --- LldbAdapter.evaluate_expression and LldbHost.observable_symbols now
# delegate to lldbbridge's shared functions; the traversal/evaluate/
# frame-walk behaviour itself is tested once, there (test_lldbbridge.py).
# These tests only prove each caller delegates and shapes its own result.

def test_adapter_evaluate_expression_delegates_to_the_shared_evaluator(monkeypatch):
    sentinel = object()
    calls = []

    def fake_evaluate_in_frame(frame, expression):
        calls.append((frame, expression))
        return sentinel

    monkeypatch.setattr(lldbbridge, 'evaluate_in_frame', fake_evaluate_in_frame)

    frame = object()
    adapter = oid_resolve_host.LldbAdapter(frame)

    assert adapter.evaluate_expression('my_expr') is sentinel
    assert calls == [(frame, 'my_expr')]


def test_adapter_evaluate_expression_propagates_runtime_error(monkeypatch):
    def raising_evaluate_in_frame(frame, expression):
        raise RuntimeError('boom')

    monkeypatch.setattr(lldbbridge, 'evaluate_in_frame', raising_evaluate_in_frame)

    adapter = oid_resolve_host.LldbAdapter(object())
    with pytest.raises(RuntimeError):
        adapter.evaluate_expression('my_expr')


def test_host_observable_symbols_shapes_pairs_into_name_type_dicts(monkeypatch):
    class _Wrapped:
        def __init__(self, type_name):
            self.type = type_name

    def fake_observable_symbols(frame, type_bridge):
        return [('a', _Wrapped('int')), ('a.b', _Wrapped('Buffer'))]

    monkeypatch.setattr(lldbbridge, 'observable_symbols', fake_observable_symbols)

    host = oid_resolve_host.LldbHost(object())

    assert host.observable_symbols() == [
        {'name': 'a', 'type': 'int'},
        {'name': 'a.b', 'type': 'Buffer'},
    ]


# --- GdbAdapter/GdbHost: the gdb-side mirror of LldbAdapter/LldbHost above.
# There is no gdbbridge.py-style shared helper for this entry point to
# delegate to (unlike lldbbridge.py for lldb), so these exercise the gdb
# idioms directly through a fake `gdb` module installed in sys.modules, the
# same mechanism fake_lldb_module uses for lldb above.

# Sentinels standing in for gdb's TYPE_CODE_STRUCT: a fake type's `code`
# defaults to a distinct, non-struct object so only types built with
# `code=_STRUCT_CODE` (below) are ever treated as struct/class-typed by
# GdbHost's member-expansion walk.
_STRUCT_CODE = object()
_NON_STRUCT_CODE = object()
# Reference types (Wrapper &) peel to their target before the struct
# check; the fake module exposes it as gdb.TYPE_CODE_REF.
_REF_CODE = object()


class _FakeGdbType:
    def __init__(self, name, code=_NON_STRUCT_CODE, fields=None):
        self._name = name
        self.code = code
        self._fields = list(fields) if fields is not None else []

    def __str__(self):
        return self._name

    def pointer(self):
        return _FakeGdbType(self._name + ' *')

    def fields(self):
        return list(self._fields)


class _FakeGdbField:
    """Stand-in for gdb.Field: one member yielded by a struct type's
    fields(), as seen by GdbHost's member-expansion walk."""

    def __init__(self, name, type_obj):
        self.name = name
        self.type = type_obj


class _FakeGdbValue:
    def __init__(self, type_name, payload=None, dereferenced=None):
        self.type = _FakeGdbType(type_name)
        self._payload = payload
        self._dereferenced = dereferenced

    def cast(self, gdb_type):
        return _FakeGdbValue(str(gdb_type), self._payload)

    def dereference(self):
        return self._dereferenced


class _FakeGdbSymbol:
    def __init__(self, name, type_name, is_variable=True, is_argument=False):
        self.name = name
        self.type = _FakeGdbType(type_name)
        self.is_variable = is_variable
        self.is_argument = is_argument


class _FakeGdbBlock:
    def __init__(self, symbols, superblock=None):
        self._symbols = symbols
        self.superblock = superblock

    def __iter__(self):
        return iter(self._symbols)


class _FakeGdbFrame:
    def __init__(self, block):
        self._block = block

    def block(self):
        return self._block


def _install_fake_gdb(monkeypatch, frame, values):
    """Fake gdb module: parse_and_eval serves `values`, selected_frame serves
    `frame`. Installed via sys.modules so `import gdb` inside the module under
    test resolves to it."""
    fake = types.ModuleType('gdb')
    fake.parse_and_eval = lambda expr: values[expr]
    fake.lookup_type = _FakeGdbType
    fake.selected_frame = lambda: frame
    fake.TYPE_CODE_STRUCT = _STRUCT_CODE
    fake.TYPE_CODE_REF = _REF_CODE
    monkeypatch.setitem(sys.modules, 'gdb', fake)
    # current_host() must not mistake an importable-but-inert lldb for the
    # host; remove any lldb stub other tests installed.
    monkeypatch.delitem(sys.modules, 'lldb', raising=False)
    return fake


def test_gdb_adapter_evaluate_and_cast(monkeypatch):
    frame = _FakeGdbFrame(_FakeGdbBlock([]))
    val = _FakeGdbValue('unsigned char *')
    _install_fake_gdb(monkeypatch, frame, {'(gray).data': val})
    adapter = oid_resolve_host.GdbAdapter()
    assert adapter.evaluate_expression('(gray).data') is val
    with pytest.raises(RuntimeError):
        adapter.evaluate_expression('missing')  # KeyError normalised
    casted = adapter.get_casted_pointer('unsigned char', val)
    assert str(casted.type) == 'unsigned char *'


def test_gdb_host_observable_symbols_ordered_and_deduped(monkeypatch):
    # Bridge hygiene: force a fresh TypeBridge and disable the user-types
    # walk-up, so a developer machine's own .oid/types.json cannot change
    # which types match (the module-level _BRIDGE cache otherwise latches
    # onto whatever was built by an earlier test).
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # 'mat' is cv::Mat (a builtin declarative type, so it is genuinely
    # observable); 'i'/'argc' are plain ints, which nothing registers as a
    # plottable buffer type, so they must be filtered out rather than
    # merely deduplicated.
    inner = _FakeGdbBlock(
        [_FakeGdbSymbol('mat', 'cv::Mat'), _FakeGdbSymbol('i', 'int')],
        superblock=_FakeGdbBlock(
            [_FakeGdbSymbol('mat', 'cv::Mat'),  # shadowed duplicate
             _FakeGdbSymbol('argc', 'int', is_variable=False, is_argument=True)]
        ),
    )
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(inner), {})
    host = oid_resolve_host.GdbHost()
    syms = host.observable_symbols()
    assert syms == [{'name': 'mat', 'type': 'cv::Mat'}]


def test_gdb_host_observable_symbols_expands_struct_members(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # `wrapper` itself is not a registered buffer type, so it must be
    # expanded rather than emitted as-is: `img` (cv::Mat) is observable and
    # surfaces as 'wrapper.img'; `count` (int) is neither observable nor a
    # struct, so it contributes nothing.
    wrapper_type = _FakeGdbType('Wrapper', code=_STRUCT_CODE, fields=[
        _FakeGdbField('img', _FakeGdbType('cv::Mat')),
        _FakeGdbField('count', _FakeGdbType('int')),
    ])
    wrapper_symbol = _FakeGdbSymbol('wrapper', 'Wrapper')
    wrapper_symbol.type = wrapper_type
    block = _FakeGdbBlock([wrapper_symbol])
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(block), {})
    host = oid_resolve_host.GdbHost()
    syms = host.observable_symbols()
    assert syms == [{'name': 'wrapper.img', 'type': 'cv::Mat'}]


def test_gdb_host_observable_symbols_expands_this_specially(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # A symbol literally named 'this' is expanded through
    # gdb.parse_and_eval('this').dereference().type.fields(), and its
    # members surface BARE ('img', never 'this.img' and never a bare
    # 'this' entry) -- the same names lldbbridge emits, so a client can
    # feed any list_observable() item straight back into resolve() under
    # either debugger (bare member names evaluate in method scope through
    # the implicit this).
    wrapper_type = _FakeGdbType('Wrapper', code=_STRUCT_CODE, fields=[
        _FakeGdbField('img', _FakeGdbType('cv::Mat')),
        _FakeGdbField('count', _FakeGdbType('int')),
    ])
    this_symbol = _FakeGdbSymbol('this', 'Wrapper *')
    this_value = _FakeGdbValue(
        'Wrapper *', dereferenced=types.SimpleNamespace(type=wrapper_type))
    block = _FakeGdbBlock([this_symbol])
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(block), {'this': this_value})
    host = oid_resolve_host.GdbHost()
    syms = host.observable_symbols()
    assert syms == [{'name': 'img', 'type': 'cv::Mat'}]


def test_current_host_routes_to_gdb(monkeypatch):
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(_FakeGdbBlock([])), {})
    host = oid_resolve_host.current_host()
    assert isinstance(host, oid_resolve_host.GdbHost)


def test_gdb_host_expands_reference_typed_aggregates(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # A Wrapper& local (any reference parameter) is field-navigated with
    # '.' exactly like a value -- the declarative engine already treats
    # T& as not-a-pointer -- so the walk peels the reference and expands
    # observable members; without the peel, reference-typed aggregates
    # would never list at all.
    wrapper_type = _FakeGdbType('Wrapper', code=_STRUCT_CODE, fields=[
        _FakeGdbField('img', _FakeGdbType('cv::Mat')),
        _FakeGdbField('count', _FakeGdbType('int')),
    ])
    ref_type = _FakeGdbType('Wrapper &', code=_REF_CODE)
    ref_type.target = lambda: wrapper_type
    ref_symbol = _FakeGdbSymbol('wrapper_ref', 'Wrapper &')
    ref_symbol.type = ref_type
    block = _FakeGdbBlock([ref_symbol])
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(block), {})
    host = oid_resolve_host.GdbHost()
    assert host.observable_symbols() == [
        {'name': 'wrapper_ref.img', 'type': 'cv::Mat'}]


def test_gdb_host_skips_unnamed_block_symbols(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # gdb can yield block symbols without a name; emitting them would put
    # a JSON null (or a None-parented member path) into the page that
    # resolve() can never evaluate. lldbbridge skips unnamed symbols the
    # same way.
    unnamed = _FakeGdbSymbol(None, 'cv::Mat')
    named = _FakeGdbSymbol('mat', 'cv::Mat')
    block = _FakeGdbBlock([unnamed, named])
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(block), {})
    host = oid_resolve_host.GdbHost()
    assert host.observable_symbols() == [{'name': 'mat', 'type': 'cv::Mat'}]


def test_gdb_host_buffer_metadata_normalizes_evaluation_errors(monkeypatch):
    # buffer_metadata evaluates through the adapter so a failed lookup
    # surfaces the adapter's uniform 'Expression "..." failed' contract --
    # resolve() stringifies whatever escapes, and a raw backend exception
    # (KeyError here, gdb.error live) is not a client-facing message.
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(_FakeGdbBlock([])), {})
    host = oid_resolve_host.GdbHost()
    with pytest.raises(RuntimeError) as excinfo:
        host.buffer_metadata('missing')
    assert 'Expression "missing" failed' in str(excinfo.value)


def test_gdb_host_walks_through_unnamed_and_base_fields(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # Anonymous aggregates and base-class subobjects contribute no path
    # segment: C++ addresses their members directly on the containing
    # object, so the emitted names must skip the nameless/base hop
    # ('outer.img', never 'outer.None.img' or 'outer.Base.base_img'
    # spelled through the base) or resolve() could never evaluate them.
    inner = _FakeGdbType('', code=_STRUCT_CODE, fields=[
        _FakeGdbField('img', _FakeGdbType('cv::Mat')),
    ])
    base = _FakeGdbType('Base', code=_STRUCT_CODE, fields=[
        _FakeGdbField('base_img', _FakeGdbType('cv::Mat')),
    ])
    base_field = _FakeGdbField('Base', base)
    base_field.is_base_class = True
    outer_type = _FakeGdbType('Outer', code=_STRUCT_CODE, fields=[
        _FakeGdbField(None, inner),
        base_field,
        _FakeGdbField('count', _FakeGdbType('int')),
    ])
    outer_symbol = _FakeGdbSymbol('outer', 'Outer')
    outer_symbol.type = outer_type
    block = _FakeGdbBlock([outer_symbol])
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(block), {})
    host = oid_resolve_host.GdbHost()
    assert host.observable_symbols() == [
        {'name': 'outer.img', 'type': 'cv::Mat'},
        {'name': 'outer.base_img', 'type': 'cv::Mat'},
    ]


def test_gdb_host_omits_this_members_shadowed_by_locals(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # C++ resolves an unqualified name to a local/argument before an
    # implicit this-member, so a bare member name colliding with ANY
    # in-scope symbol would list the member while resolve() evaluates the
    # local -- the wrong object. Shadowed members are omitted; every
    # emitted name resolves to exactly what was listed.
    wrapper_type = _FakeGdbType('Wrapper', code=_STRUCT_CODE, fields=[
        _FakeGdbField('img', _FakeGdbType('cv::Mat')),
        _FakeGdbField('img2', _FakeGdbType('cv::Mat')),
    ])
    this_symbol = _FakeGdbSymbol('this', 'Wrapper *')
    this_value = _FakeGdbValue(
        'Wrapper *', dereferenced=types.SimpleNamespace(type=wrapper_type))
    shadowing_local = _FakeGdbSymbol('img', 'int')  # non-observable local
    block = _FakeGdbBlock([this_symbol, shadowing_local])
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(block), {'this': this_value})
    host = oid_resolve_host.GdbHost()
    assert host.observable_symbols() == [{'name': 'img2', 'type': 'cv::Mat'}]


def test_gdb_host_skips_an_unevaluable_this_and_lists_the_rest(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # `this` is in scope but not evaluable right now (optimized out, frame
    # prologue, dead frame): its expansion must be skipped, never fatal --
    # list_observable turns any escaped exception into a full-page error,
    # which would hide every other observable symbol.
    this_symbol = _FakeGdbSymbol('this', 'Wrapper *')
    mat_symbol = _FakeGdbSymbol('mat', 'cv::Mat')
    block = _FakeGdbBlock([this_symbol, mat_symbol])
    # No 'this' entry in the values dict: parse_and_eval('this') raises.
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(block), {})
    host = oid_resolve_host.GdbHost()
    assert host.observable_symbols() == [{'name': 'mat', 'type': 'cv::Mat'}]


def test_gdb_host_without_a_stopped_frame_names_the_condition(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # Before `run` (or after the inferior exits) gdb raises from
    # selected_frame(); the host must surface the same routine-condition
    # message the lldb path uses, not an opaque internal error.
    fake = _install_fake_gdb(monkeypatch, _FakeGdbFrame(_FakeGdbBlock([])), {})

    def no_frame():
        raise RuntimeError('No frame is currently selected.')

    fake.selected_frame = no_frame
    host = oid_resolve_host.GdbHost()
    with pytest.raises(RuntimeError) as excinfo:
        host.observable_symbols()
    assert 'no stopped frame' in str(excinfo.value).lower()


def test_current_host_rejects_gdb_without_the_full_walk_api(monkeypatch):
    # The probe must require every attribute the gdb path reads without a
    # guard: a module carrying the rest of the surface but no
    # TYPE_CODE_REF would pass selection and then crash mid-walk when
    # reference peeling runs.
    monkeypatch.delitem(sys.modules, 'lldb', raising=False)
    partial = types.ModuleType('gdb')
    partial.parse_and_eval = lambda expr: None
    partial.lookup_type = lambda name: None
    partial.selected_frame = lambda: None
    partial.TYPE_CODE_STRUCT = object()
    monkeypatch.setitem(sys.modules, 'gdb', partial)
    with pytest.raises(RuntimeError) as excinfo:
        oid_resolve_host.current_host()
    assert 'no supported debugger' in str(excinfo.value).lower()


def test_current_host_with_inert_gdb_names_no_debugger(monkeypatch):
    # A module merely NAMED gdb (a leftover stub, a foreign package
    # shadowing the name) does not host this interpreter: without gdb's
    # actual API the router must end on the terminal no-debugger error
    # instead of returning a GdbHost that fails later with AttributeError
    # on its first use.
    monkeypatch.delitem(sys.modules, 'lldb', raising=False)
    monkeypatch.setitem(sys.modules, 'gdb', types.ModuleType('gdb'))
    with pytest.raises(RuntimeError) as excinfo:
        oid_resolve_host.current_host()
    assert 'no supported debugger' in str(excinfo.value).lower()
