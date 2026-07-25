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


def test_current_host_with_gdb_present_names_gdb_not_no_debugger(monkeypatch):
    # gdb is a real, supported backend elsewhere in this repo
    # (debuggers/gdbbridge.py); this entry point just doesn't serve it
    # yet. The error must say that, not claim no debugger is available.
    monkeypatch.delitem(sys.modules, 'lldb', raising=False)
    monkeypatch.setitem(sys.modules, 'gdb', types.ModuleType('gdb'))
    with pytest.raises(RuntimeError) as excinfo:
        oid_resolve_host.current_host()
    message = str(excinfo.value).lower()
    assert 'gdb' in message
    assert 'no supported debugger' not in message


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
    inert_lldb = types.ModuleType('lldb')
    monkeypatch.setitem(sys.modules, 'lldb', inert_lldb)
    monkeypatch.setitem(sys.modules, 'gdb', types.ModuleType('gdb'))

    with pytest.raises(RuntimeError) as excinfo:
        oid_resolve_host.current_host()

    message = str(excinfo.value).lower()
    assert 'gdb' in message
    assert 'lldb' not in message
    assert 'no supported debugger' not in message


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
