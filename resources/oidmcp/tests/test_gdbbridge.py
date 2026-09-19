# -*- coding: utf-8 -*-

"""GdbBridge's symbol listing -- the walk behind the viewer's buffer list
and the `plot` command in a plain gdb session.

oid_resolve_host.py serves the IDE clients through its own gdb walk and is
tested in test_oid_resolve_host.py; this file covers the older bridge, which
answers the same question for a debugger session driving OID directly. Both
must name a buffer the way the frame evaluates it.

gdbbridge imports `gdb` at module scope, which no plain pytest run can
provide, so each test installs a fake module and imports the bridge behind
it. The module is dropped from sys.modules afterwards: it binds its `gdb`
name permanently on first import, and a later test that installed a
different fake would otherwise keep reading this one.
"""

import importlib
import sys
import types

import pytest

STRUCT_CODE = object()
TYPEDEF_CODE = object()
UNION_CODE = object()
SCALAR_CODE = object()


class FakeGdbType:
    def __init__(self, name, code=SCALAR_CODE, fields=None, target=None):
        self._name = name
        self.code = code
        self._fields = list(fields) if fields is not None else []
        self._target = target

    def __str__(self):
        return self._name

    def fields(self):
        return list(self._fields)

    def strip_typedefs(self):
        return self._target if self._target is not None else self


class FakeGdbField:
    def __init__(self, name, type_obj, is_base_class=False):
        self.name = name
        self.type = type_obj
        self.is_base_class = is_base_class


class FakeGdbSymbol:
    def __init__(self, name, type_obj, is_variable=True, is_argument=False):
        self.name = name
        self.type = type_obj
        self.is_variable = is_variable
        self.is_argument = is_argument


class FakeGdbBlock:
    def __init__(self, symbols, superblock=None):
        self._symbols = symbols
        self.superblock = superblock

    def __iter__(self):
        return iter(self._symbols)


class FakeGdbFrame:
    def __init__(self, block):
        self._block = block

    def block(self):
        return self._block


class FakeTypeBridge:
    """A symbol is observable iff its type name says so."""

    def __init__(self, observable_typenames):
        self._observable = set(observable_typenames)

    def is_symbol_observable(self, symbol, _name):
        return str(symbol.type) in self._observable


@pytest.fixture
def bridge_module(monkeypatch):
    fake = types.ModuleType('gdb')
    fake.TYPE_CODE_STRUCT = STRUCT_CODE
    fake.TYPE_CODE_UNION = UNION_CODE
    fake.Command = object
    fake.COMMAND_DATA = object()
    fake.COMPLETE_SYMBOL = object()
    fake.events = types.SimpleNamespace(
        stop=types.SimpleNamespace(connect=lambda _handler: None),
        exited=types.SimpleNamespace(connect=lambda _handler: None))
    monkeypatch.setitem(sys.modules, 'gdb', fake)
    monkeypatch.delitem(sys.modules, 'oidscripts.debuggers.gdbbridge',
                        raising=False)
    module = importlib.import_module('oidscripts.debuggers.gdbbridge')
    yield module
    sys.modules.pop('oidscripts.debuggers.gdbbridge', None)


def _observable_names(module, symbol, observable_typenames):
    """Run the bridge's member walk over one symbol, without building a
    GdbBridge (whose constructor registers gdb event handlers and starts
    its event-loop thread)."""
    bridge = module.GdbBridge.__new__(module.GdbBridge)
    bridge._type_bridge = FakeTypeBridge(observable_typenames)
    found = set()
    bridge._get_observable_children_members(symbol, found)
    return found


def test_a_base_class_subobject_contributes_no_path_segment(bridge_module):
    # 'holder.Base.baseMember' evaluates nowhere (issue #1102, gdb half).
    base = FakeGdbType('Base', code=STRUCT_CODE, fields=[
        FakeGdbField('baseMember', FakeGdbType('Buffer')),
    ])
    holder = FakeGdbSymbol('holder', FakeGdbType(
        'Derived', code=STRUCT_CODE, fields=[
            FakeGdbField('Base', base, is_base_class=True),
            FakeGdbField('member', FakeGdbType('Buffer')),
        ]))

    found = _observable_names(bridge_module, holder, {'Buffer'})

    assert found == {'holder.baseMember', 'holder.member'}


def test_an_anonymous_aggregate_contributes_no_path_segment(bridge_module):
    # An unnamed field interpolates into the literal segment 'None'.
    anonymous = FakeGdbType('', code=UNION_CODE, fields=[
        FakeGdbField('anonU', FakeGdbType('Buffer')),
    ])
    holder = FakeGdbSymbol('holder', FakeGdbType(
        'Holder', code=STRUCT_CODE, fields=[FakeGdbField(None, anonymous)]))

    found = _observable_names(bridge_module, holder, {'Buffer'})

    assert found == {'holder.anonU'}


def test_a_buffer_held_directly_by_this_is_listed(bridge_module):
    # A field passed as its own parent loses its name from the path, and
    # the members of `this` are named bare.
    image = FakeGdbField('image', FakeGdbType('Buffer'))
    base = FakeGdbType('Base', code=STRUCT_CODE, fields=[
        FakeGdbField('baseMember', FakeGdbType('Buffer')),
    ])
    this_type = FakeGdbType('Holder', code=STRUCT_CODE, fields=[
        FakeGdbField('Base', base, is_base_class=True),
        image,
    ])
    bridge_module.gdb.parse_and_eval = lambda _expr: types.SimpleNamespace(
        dereference=lambda: FakeGdbSymbol('*this', this_type))
    bridge_module.gdb.selected_frame = lambda: FakeGdbFrame(
        FakeGdbBlock([FakeGdbSymbol('this', FakeGdbType('Holder *'))]))

    bridge = bridge_module.GdbBridge.__new__(bridge_module.GdbBridge)
    bridge._type_bridge = FakeTypeBridge({'Buffer'})
    found = set()
    bridge._add_observable_symbol(FakeGdbSymbol('this', FakeGdbType(
        'Holder *')), 'this', found)

    assert found == {'image', 'baseMember'}


def test_a_named_union_is_descended_into(bridge_module):
    # A union's members are spelled exactly like a struct's.
    union = FakeGdbType('Payload', code=UNION_CODE, fields=[
        FakeGdbField('image', FakeGdbType('Buffer')),
    ])
    holder = FakeGdbSymbol('holder', FakeGdbType(
        'Holder', code=STRUCT_CODE, fields=[FakeGdbField('payload', union)]))

    found = _observable_names(bridge_module, holder, {'Buffer'})

    assert found == {'holder.payload.image'}


def test_a_typedef_wrapped_union_is_descended_into(bridge_module):
    # A `typedef union {...} Alias;` local looks scalar until stripped.
    union = FakeGdbType('Payload', code=UNION_CODE, fields=[
        FakeGdbField('image', FakeGdbType('Buffer')),
    ])
    alias = FakeGdbType('PayloadAlias', code=TYPEDEF_CODE, target=union)
    holder = FakeGdbSymbol('holder', FakeGdbType(
        'Holder', code=STRUCT_CODE, fields=[FakeGdbField('payload', alias)]))

    found = _observable_names(bridge_module, holder, {'Buffer'})

    assert found == {'holder.payload.image'}


def test_members_of_this_surface_bare(bridge_module):
    # gdb accepts `this.image`; three walks disagreeing is the defect.
    image = FakeGdbField('image', FakeGdbType('Buffer'))
    base = FakeGdbType('Base', code=STRUCT_CODE, fields=[
        FakeGdbField('baseMember', FakeGdbType('Buffer')),
    ])
    this_type = FakeGdbType('Holder', code=STRUCT_CODE, fields=[
        FakeGdbField('Base', base, is_base_class=True),
        image,
    ])
    bridge_module.gdb.parse_and_eval = lambda _expr: types.SimpleNamespace(
        dereference=lambda: FakeGdbSymbol('*this', this_type))
    bridge_module.gdb.selected_frame = lambda: FakeGdbFrame(
        FakeGdbBlock([FakeGdbSymbol('this', FakeGdbType('Holder *'))]))

    bridge = bridge_module.GdbBridge.__new__(bridge_module.GdbBridge)
    bridge._type_bridge = FakeTypeBridge({'Buffer'})
    found = set()
    bridge._add_observable_symbol(FakeGdbSymbol('this', FakeGdbType(
        'Holder *')), 'this', found)

    assert found == {'image', 'baseMember'}


def test_a_this_member_shadowed_by_a_local_is_not_listed(bridge_module):
    # A bare name a local carries evaluates to that variable.
    image = FakeGdbField('image', FakeGdbType('Buffer'))
    this_type = FakeGdbType('Holder', code=STRUCT_CODE, fields=[image])
    bridge_module.gdb.parse_and_eval = lambda _expr: types.SimpleNamespace(
        dereference=lambda: FakeGdbSymbol('*this', this_type))
    local = FakeGdbSymbol('image', FakeGdbType('int'))
    bridge_module.gdb.selected_frame = lambda: FakeGdbFrame(
        FakeGdbBlock([local, FakeGdbSymbol('this', FakeGdbType('Holder *'))]))

    bridge = bridge_module.GdbBridge.__new__(bridge_module.GdbBridge)
    bridge._type_bridge = FakeTypeBridge({'Buffer'})
    found = set()
    bridge._add_observable_symbol(FakeGdbSymbol('this', FakeGdbType(
        'Holder *')), 'this', found)

    assert found == set()


def test_an_unshadowed_this_member_is_still_listed(bridge_module):
    image = FakeGdbField('image', FakeGdbType('Buffer'))
    this_type = FakeGdbType('Holder', code=STRUCT_CODE, fields=[image])
    bridge_module.gdb.parse_and_eval = lambda _expr: types.SimpleNamespace(
        dereference=lambda: FakeGdbSymbol('*this', this_type))
    local = FakeGdbSymbol('width', FakeGdbType('int'))
    bridge_module.gdb.selected_frame = lambda: FakeGdbFrame(
        FakeGdbBlock([local, FakeGdbSymbol('this', FakeGdbType('Holder *'))]))

    bridge = bridge_module.GdbBridge.__new__(bridge_module.GdbBridge)
    bridge._type_bridge = FakeTypeBridge({'Buffer'})
    found = set()
    bridge._add_observable_symbol(FakeGdbSymbol('this', FakeGdbType(
        'Holder *')), 'this', found)

    assert found == {'image'}
