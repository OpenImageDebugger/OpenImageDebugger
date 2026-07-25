# -*- coding: utf-8 -*-
"""Numeric and pointer coercion for declarative fields.

Covers the LLDB-specific paths where an evaluated result is an integer-typed
``SBValue`` whose textual form may be hex, or a scalar integer that already
holds a buffer address. Both regressed under LLDB: ``int('0x10')`` raised
``ValueError``, and ``get_casted_pointer()`` took ``AddressOf()`` of an
integer value (the address of the temporary) instead of the value itself.
"""

from oidscripts.oidtypes.declarative import _to_int


# --- declarative._to_int -----------------------------------------------------

def test_to_int_decimal_string():
    assert _to_int('640') == 640


def test_to_int_hex_string():
    # LLDB renders some integer results in hex; the coercion must accept it.
    assert _to_int('0x10') == 16


def test_to_int_negative_string():
    assert _to_int('-5') == -5


def test_to_int_passthrough_int():
    assert _to_int(42) == 42


# --- LLDB SymbolWrapper numeric / pointer accessors --------------------------
#
# lldbbridge hard-imports the `lldb` module, which is unavailable off a live
# debugger. conftest.py installs the shared stub carrying the constant this
# file reads (eTypeIsInteger) before any test module is collected, so the
# pure Python of SymbolWrapper can be exercised here without a local one.

import lldb
from oidscripts.debuggers.lldbbridge import SymbolWrapper


class _FakeSBType:
    def __init__(self, name='int', type_flags=0, valid=True):
        self._name = name
        self._flags = type_flags
        self._valid = valid

    def IsValid(self):
        return self._valid

    def GetCanonicalType(self):
        return self

    def GetName(self):
        return self._name

    def GetTypeFlags(self):
        return self._flags


class _FakeSBValue:
    def __init__(self, *, typename='int', type_flags=0, value=None,
                 signed=0, unsigned=0, is_pointer=False, address_of=None):
        self._typename = typename
        self._type = _FakeSBType(typename, type_flags)
        self._value = value
        self._signed = signed
        self._unsigned = unsigned
        self._is_pointer = is_pointer
        self._address_of = address_of

    def GetType(self):
        return self._type

    def GetTypeName(self):
        return self._typename

    def GetValue(self):
        return self._value

    def GetValueAsSigned(self):
        return self._signed

    def GetValueAsUnsigned(self):
        return self._unsigned

    def TypeIsPointerType(self):
        return self._is_pointer

    def AddressOf(self):
        return self._address_of


def test_symbolwrapper_int_decimal():
    wrapper = SymbolWrapper(_FakeSBValue(value='640'))
    assert int(wrapper) == 640


def test_symbolwrapper_int_hex():
    # The reported bug: int('0x10') raised ValueError; base-0 parse fixes it.
    wrapper = SymbolWrapper(_FakeSBValue(value='0x10'))
    assert int(wrapper) == 16


def test_symbolwrapper_int_falls_back_to_scalar_accessor():
    # No parseable textual value (a char literal, None, ...): use the scalar
    # accessor rather than raising.
    wrapper = SymbolWrapper(_FakeSBValue(value=None, signed=42))
    assert int(wrapper) == 42


def test_get_casted_pointer_pointer_type():
    wrapper = SymbolWrapper(_FakeSBValue(is_pointer=True, unsigned=0xDEADBEEF))
    assert wrapper.get_casted_pointer() == 0xDEADBEEF


def test_get_casted_pointer_integer_value_is_the_address():
    # The reported bug: an integer-typed result (uintptr_t / numeric literal)
    # must be read as its own value, NOT AddressOf() of the temporary holding
    # it. The address_of sentinel would win if the old branch were taken.
    sentinel_address_of = _FakeSBValue(unsigned=0x9999)
    wrapper = SymbolWrapper(_FakeSBValue(
        type_flags=lldb.eTypeIsInteger,
        unsigned=0x1000,
        address_of=sentinel_address_of))
    assert wrapper.get_casted_pointer() == 0x1000


def test_get_casted_pointer_non_integer_uses_address_of():
    # A struct/array result keeps the historical behaviour: take its address.
    address_of = _FakeSBValue(unsigned=0x7777)
    wrapper = SymbolWrapper(_FakeSBValue(
        typename='MyStruct', type_flags=0, address_of=address_of))
    assert wrapper.get_casted_pointer() == 0x7777
