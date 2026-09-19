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
UNION_CODE = object()
SCALAR_CODE = object()


class FakeGdbType:
    def __init__(self, name, code=SCALAR_CODE, fields=None):
        self._name = name
        self.code = code
        self._fields = list(fields) if fields is not None else []

    def __str__(self):
        return self._name

    def fields(self):
        return list(self._fields)


class FakeGdbField:
    def __init__(self, name, type_obj, is_base_class=False):
        self.name = name
        self.type = type_obj
        self.is_base_class = is_base_class


class FakeGdbSymbol:
    def __init__(self, name, type_obj):
        self.name = name
        self.type = type_obj


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
    # gdb reports a base-class subobject as a field whose name is the base
    # type's, but C++ addresses an inherited member directly on the derived
    # object, so 'holder.Base.baseMember' is an expression no frame can
    # evaluate (issue #1102, the gdb half).
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
    # gdb gives an anonymous union or struct a field with name None, which
    # f-string interpolation turns into the literal segment 'None'.
    anonymous = FakeGdbType('', code=UNION_CODE, fields=[
        FakeGdbField('anonU', FakeGdbType('Buffer')),
    ])
    holder = FakeGdbSymbol('holder', FakeGdbType(
        'Holder', code=STRUCT_CODE, fields=[FakeGdbField(None, anonymous)]))

    found = _observable_names(bridge_module, holder, {'Buffer'})

    assert found == {'holder.anonU'}


def test_a_buffer_held_directly_by_this_is_listed(bridge_module):
    # The `this` branch fed each FIELD of the pointee to the member walk
    # with 'this' as the parent, so a field's own name never entered the
    # name: a buffer held directly by `this` was skipped and the walk
    # reported its sub-fields instead. Hand the walk the dereferenced
    # value, so every member is named under 'this'.
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

    bridge = bridge_module.GdbBridge.__new__(bridge_module.GdbBridge)
    bridge._type_bridge = FakeTypeBridge({'Buffer'})
    found = set()
    bridge._add_observable_symbol(FakeGdbSymbol('this', FakeGdbType(
        'Holder *')), 'this', found)

    assert found == {'this.image', 'this.baseMember'}


def test_a_named_union_is_descended_into(bridge_module):
    # A union's members are spelled exactly like a struct's, so refusing to
    # descend into one hides every buffer a union holds.
    union = FakeGdbType('Payload', code=UNION_CODE, fields=[
        FakeGdbField('image', FakeGdbType('Buffer')),
    ])
    holder = FakeGdbSymbol('holder', FakeGdbType(
        'Holder', code=STRUCT_CODE, fields=[FakeGdbField('payload', union)]))

    found = _observable_names(bridge_module, holder, {'Buffer'})

    assert found == {'holder.payload.image'}
