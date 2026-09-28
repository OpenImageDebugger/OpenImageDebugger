"""The debugger-facing half: bridge adapter plus host selection."""

import sys
import types

import pytest
from conftest import FakeFrame, fake_lldb_module
from oidscripts import oid_resolve_host
from oidscripts.debuggers import lldbbridge

# lldbbridge binds its module-level `lldb` at first import, so only conftest's
# import-time stub must be unique; per-test monkeypatched stubs are fine.


def test_current_host_without_a_debugger_raises_a_named_error(monkeypatch):
    monkeypatch.delitem(sys.modules, 'lldb', raising=False)
    monkeypatch.delitem(sys.modules, 'gdb', raising=False)
    with pytest.raises(RuntimeError) as excinfo:
        oid_resolve_host.current_host()
    assert 'no supported debugger' in str(excinfo.value).lower()


def test_current_host_with_gdb_present_returns_a_gdb_host(monkeypatch):
    # Routing requires the module to carry gdb's actual Python API, not just
    # the name; _install_fake_gdb (below, with the gdb tests) provides it.
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
    # `import lldb` can succeed just because the package is installed, with
    # no session marker set; the probe must fall through to the real gdb.
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(_FakeGdbBlock([])), {})
    inert_lldb = types.ModuleType('lldb')
    monkeypatch.setitem(sys.modules, 'lldb', inert_lldb)

    host = oid_resolve_host.current_host()

    assert isinstance(host, oid_resolve_host.GdbHost)


def test_current_host_with_lldb_present_but_no_stopped_frame_names_lldb_not_no_debugger(monkeypatch):
    # lldb loaded with no stopped frame is routine, not "no debugger at all":
    # falling through to the gdb probe would end on a misleading message.
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


# The traversal/evaluate/frame-walk behaviour is tested once, in
# test_lldbbridge.py; these only prove each caller delegates to it.

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


# There is no gdbbridge-style shared helper for this entry point to delegate
# to, so these drive the gdb idioms through a fake `gdb` in sys.modules.

# Sentinels for gdb's TYPE_CODE_STRUCT: a fake type's `code` defaults to a
# distinct non-struct object, so only code=_STRUCT_CODE is ever expanded.
_STRUCT_CODE = object()
_NON_STRUCT_CODE = object()
# Reference types (Wrapper &) peel to their target before the struct
# check; the fake module exposes it as gdb.TYPE_CODE_REF.
_REF_CODE = object()
# Exposed by the fake module as gdb.TYPE_CODE_UNION.
_UNION_CODE = object()


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
    def __init__(self, symbols, superblock=None, is_static=False,
                 is_global=False):
        self._symbols = symbols
        self.superblock = superblock
        self.is_static = is_static
        self.is_global = is_global

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
    fake.TYPE_CODE_UNION = _UNION_CODE
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
    # Fresh TypeBridge with the user-types walk-up off, so a developer's own
    # .oid/types.json (or an earlier test's latched _BRIDGE) cannot match.
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # 'mat' is a builtin declarative type; 'i'/'argc' are plain ints nothing
    # registers as plottable, so they are filtered, not merely deduplicated.
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
    # `wrapper` is not a registered buffer type, so it is expanded, not
    # emitted: `img` surfaces as 'wrapper.img', `count` contributes nothing.
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
    # Members of 'this' surface BARE ('img', never 'this.img'), matching what
    # lldbbridge emits, so any listed name feeds straight back into resolve().
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


def test_gdb_host_expands_reference_typed_aggregates(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # T& is field-navigated with '.' like a value (the declarative engine
    # treats it as not-a-pointer), so the walk must peel the reference.
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
    # gdb can yield block symbols without a name; emitting one puts a JSON
    # null into the page that resolve() can never evaluate.
    unnamed = _FakeGdbSymbol(None, 'cv::Mat')
    named = _FakeGdbSymbol('mat', 'cv::Mat')
    block = _FakeGdbBlock([unnamed, named])
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(block), {})
    host = oid_resolve_host.GdbHost()
    assert host.observable_symbols() == [{'name': 'mat', 'type': 'cv::Mat'}]


def test_gdb_host_buffer_metadata_normalizes_evaluation_errors(monkeypatch):
    # buffer_metadata evaluates through the adapter, so a failed lookup
    # (KeyError from the empty eval table here, gdb.error live) surfaces as
    # 'Expression "..." failed'; resolve() would stringify the raw exception.
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(_FakeGdbBlock([])), {})
    host = oid_resolve_host.GdbHost()
    with pytest.raises(RuntimeError) as excinfo:
        host.buffer_metadata('missing')
    assert 'Expression "missing" failed' in str(excinfo.value)


def test_gdb_host_walks_through_unnamed_and_base_fields(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # C++ addresses members of anonymous aggregates and base subobjects on
    # the containing object, so the name must skip that hop ('outer.img').
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


def test_gdb_host_descends_into_a_union(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # Structs-only descent hides union members outright; lldb lists them.
    named = _FakeGdbType('Payload', code=_UNION_CODE, fields=[
        _FakeGdbField('img', _FakeGdbType('cv::Mat')),
    ])
    anonymous = _FakeGdbType('', code=_UNION_CODE, fields=[
        _FakeGdbField('anon_img', _FakeGdbType('cv::Mat')),
    ])
    outer_type = _FakeGdbType('Outer', code=_STRUCT_CODE, fields=[
        _FakeGdbField('payload', named),
        _FakeGdbField(None, anonymous),
    ])
    outer_symbol = _FakeGdbSymbol('outer', 'Outer')
    outer_symbol.type = outer_type
    block = _FakeGdbBlock([outer_symbol])
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(block), {})
    host = oid_resolve_host.GdbHost()
    assert host.observable_symbols() == [
        {'name': 'outer.payload.img', 'type': 'cv::Mat'},
        {'name': 'outer.anon_img', 'type': 'cv::Mat'},
    ]


def test_gdb_host_omits_this_members_shadowed_by_locals(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # C++ resolves an unqualified name to a local before an implicit this-
    # member, so a colliding bare member would resolve to the wrong object.
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
    # `this` can be in scope but unevaluable (optimized out, prologue); a
    # fatal expansion would hide every other symbol behind a page error.
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
    # selected_frame(); that must read as the lldb path's routine message.
    fake = _install_fake_gdb(monkeypatch, _FakeGdbFrame(_FakeGdbBlock([])), {})

    def no_frame():
        raise RuntimeError('No frame is currently selected.')

    fake.selected_frame = no_frame
    host = oid_resolve_host.GdbHost()
    with pytest.raises(RuntimeError) as excinfo:
        host.observable_symbols()
    assert 'no stopped frame' in str(excinfo.value).lower()


def test_current_host_rejects_gdb_without_the_full_walk_api(monkeypatch):
    # The probe must require every attribute the gdb path reads unguarded: a
    # module missing TYPE_CODE_REF would pass selection, then crash mid-walk.
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
    # A module merely NAMED gdb (a leftover stub, a shadowing package) does
    # not host this interpreter; a GdbHost would AttributeError on first use.
    monkeypatch.delitem(sys.modules, 'lldb', raising=False)
    monkeypatch.setitem(sys.modules, 'gdb', types.ModuleType('gdb'))
    with pytest.raises(RuntimeError) as excinfo:
        oid_resolve_host.current_host()
    assert 'no supported debugger' in str(excinfo.value).lower()


def test_gdb_host_lets_a_member_beat_a_global_of_the_same_name(monkeypatch):
    monkeypatch.setattr(oid_resolve_host, '_BRIDGE', None)
    monkeypatch.setenv('OID_TYPES_PATH', '')
    # Static and global blocks are searched AFTER field-of-this.
    this_type = _FakeGdbType('Holder', code=_STRUCT_CODE, fields=[
        _FakeGdbField('img', _FakeGdbType('cv::Mat')),
    ])
    this_value = _FakeGdbValue('Holder *', dereferenced=_FakeGdbValue('Holder'))
    this_value._dereferenced.type = this_type
    global_block = _FakeGdbBlock([_FakeGdbSymbol('img', 'int')],
                                 is_global=True)
    frame_block = _FakeGdbBlock([_FakeGdbSymbol('this', 'Holder *')],
                                superblock=global_block)
    _install_fake_gdb(monkeypatch, _FakeGdbFrame(frame_block),
                      {'this': this_value})
    host = oid_resolve_host.GdbHost()
    assert host.observable_symbols() == [{'name': 'img', 'type': 'cv::Mat'}]
