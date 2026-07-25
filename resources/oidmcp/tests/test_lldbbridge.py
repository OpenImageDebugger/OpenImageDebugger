"""lldbbridge's module-level shared functions -- frame_from_debugger(),
evaluate_in_frame(), observable_symbols() -- plus the thin LldbBridge
wrappers around them. oid_resolve_host.py delegates to these same
functions; see test_oid_resolve_host.py for its (smaller) delegation and
result-shaping tests.

conftest.py installs the one shared `lldb` stub every test in this suite
sees; nothing module-level is needed here.
"""

import sys

import pytest
from conftest import (
    FakeDebugger,
    FakeFrame,
    FakeSBType,
    FakeSBValue,
    FakeTarget,
    FakeTypeBridge,
    fake_lldb_module,
)
from oidscripts.debuggers import lldbbridge
from oidscripts.debuggers.lldbbridge import (
    LldbBridge,
    evaluate_in_frame,
    frame_from_debugger,
    observable_symbols,
)

# --- frame_from_debugger(): the process -> thread -> frame walk, including
# the stop-reason filtering that used to live only in LldbBridge._get_thread.

def test_frame_from_debugger_returns_the_stopped_threads_frame(monkeypatch):
    expected_frame = FakeFrame()
    fake_lldb = fake_lldb_module(selected_frame=expected_frame)
    monkeypatch.setitem(sys.modules, 'lldb', fake_lldb)

    assert frame_from_debugger(fake_lldb.debugger) is expected_frame


def test_frame_from_debugger_returns_none_without_a_stopped_thread(monkeypatch):
    # No selected_frame: neither thread carries a real stop reason.
    fake_lldb = fake_lldb_module()
    monkeypatch.setitem(sys.modules, 'lldb', fake_lldb)

    assert not frame_from_debugger(fake_lldb.debugger)


def test_frame_from_debugger_guards_falsy_debugger_target_and_process(monkeypatch):
    fake_lldb = fake_lldb_module()
    monkeypatch.setitem(sys.modules, 'lldb', fake_lldb)

    assert not frame_from_debugger(None)
    assert not frame_from_debugger(FakeDebugger(None))
    assert not frame_from_debugger(FakeDebugger(FakeTarget(None)))


# --- evaluate_in_frame(): the FindVariable fast path for a plain
# identifier, EvaluateExpression() otherwise, and the error guards around
# both.

class _FakeSBError:
    def __init__(self, fail=False, message=None):
        self._fail = fail
        self._message = message

    def Fail(self):
        return self._fail

    def GetCString(self):
        return self._message


class _FakeEvalValue:
    """Just enough of lldb.SBValue for SymbolWrapper's constructor plus the
    checks in evaluate_in_frame."""

    def __init__(self, valid=True, error=None):
        self._valid = valid
        self._error = error if error is not None else _FakeSBError()

    def __bool__(self):
        return self._valid

    def IsValid(self):
        return self._valid

    def GetType(self):
        return FakeSBType('int')

    def GetTypeName(self):
        return 'int'

    def GetError(self):
        return self._error


class _FakeErrorlessInvalidValue:
    """A truthy-but-invalid SB value whose GetError() returns None outright,
    rather than a non-failing error object. Composing the RuntimeError
    message must not crash trying to call None.GetCString()."""

    def __bool__(self):
        return True

    def IsValid(self):
        return False

    def GetError(self):
        return None


def test_evaluate_in_frame_prefers_find_variable_for_a_plain_identifier():
    class Frame:
        def __init__(self):
            self.evaluate_called = False

        def FindVariable(self, name):
            assert name == 'my_var'
            return _FakeEvalValue(valid=True)

        def EvaluateExpression(self, expr):
            self.evaluate_called = True
            raise AssertionError(
                'a plain identifier must resolve via FindVariable, not '
                'the expression JIT')

    frame = Frame()

    result = evaluate_in_frame(frame, 'my_var')

    assert result is not None
    assert not frame.evaluate_called


def test_evaluate_in_frame_falls_back_to_evaluate_expression_when_find_variable_is_invalid():
    class Frame:
        def FindVariable(self, name):
            return _FakeEvalValue(valid=False)

        def EvaluateExpression(self, expr):
            return _FakeEvalValue(valid=True)

    result = evaluate_in_frame(Frame(), 'my_var')

    assert result is not None


def test_evaluate_in_frame_skips_find_variable_for_a_non_identifier_expression():
    class Frame:
        def __init__(self):
            self.find_variable_called = False

        def FindVariable(self, name):
            self.find_variable_called = True
            raise AssertionError(
                'a dotted/complex expression is not a plain identifier '
                'and must go straight to EvaluateExpression')

        def EvaluateExpression(self, expr):
            return _FakeEvalValue(valid=True)

    frame = Frame()

    evaluate_in_frame(frame, 'this.image')

    assert not frame.find_variable_called


def test_evaluate_in_frame_raises_runtime_error_when_get_error_returns_none():
    class Frame:
        def EvaluateExpression(self, expr):
            return _FakeErrorlessInvalidValue()

    frame = Frame()

    with pytest.raises(RuntimeError):
        evaluate_in_frame(frame, '1 + 1')


def test_evaluate_in_frame_raises_runtime_error_for_an_invalid_but_truthy_value():
    # A value can be truthy but still invalid; a non-failing error object
    # (Fail() == False) must not mask that.
    class Frame:
        def EvaluateExpression(self, expr):
            return _FakeEvalValue(valid=False, error=_FakeSBError(fail=False))

    frame = Frame()

    with pytest.raises(RuntimeError):
        evaluate_in_frame(frame, '1 + 1')


def test_evaluate_in_frame_raises_runtime_error_for_a_falsy_result():
    class DeadFrame:
        def EvaluateExpression(self, expr):
            return None

    frame = DeadFrame()

    with pytest.raises(RuntimeError):
        evaluate_in_frame(frame, '1 + 1')


# --- observable_symbols(): top-level frame variables plus the recursive
# observable-member traversal, and its cycle guard.

class _VariablesFrame:
    """Minimal SBFrame stand-in: GetVariables() returns a fixed list."""

    def __init__(self, variables):
        self._variables = variables

    def GetVariables(self, *args):
        return self._variables


def _observable_names(root, observable_typenames):
    """Run observable_symbols() over a fake single-variable frame and
    return the qualified names it found."""
    frame = _VariablesFrame([root])
    bridge = FakeTypeBridge(observable_typenames)
    return {name for name, _wrapped in observable_symbols(frame, bridge)}


def test_observable_symbols_finds_a_buffer_three_levels_deep():
    # this.wrapper.inner.image -- must be found by its full dotted name.
    # The old guard marked the *child's* type just before recursing into
    # it, so the recursive call's very first check tripped on that same
    # mark and returned at once -- a member's name (e.g. "image") almost
    # never equals its own type name (e.g. "cv::Mat"), so the mark nearly
    # always landed and the walk nearly always stopped after one hop.
    image = FakeSBValue('image', 'Buffer')
    inner = FakeSBValue('inner', 'Inner', children=[image])
    wrapper = FakeSBValue('wrapper', 'Wrapper', children=[inner])
    root = FakeSBValue('root', 'Root', children=[wrapper])

    found = _observable_names(root, observable_typenames={'Buffer'})

    assert found == {'root.wrapper.inner.image'}


def test_observable_symbols_terminates_on_a_genuine_cycle():
    # node_b points back to node_a: an actual cycle, not just a deep
    # chain. The guard must still stop the walk (no RecursionError, no
    # hang) now that it no longer stops after a single hop regardless.
    node_a = FakeSBValue('a', 'NodeA')
    node_b = FakeSBValue('b', 'NodeB', children=[node_a])
    node_a.set_children([node_b])

    found = _observable_names(node_a, observable_typenames={'Buffer'})

    assert found == set()


def test_observable_symbols_visits_both_siblings_through_the_same_type():
    # Two sibling branches ('a' and 'b') share the same intermediate type
    # ('Wrapper'). A visited-set *shared* across siblings would let the
    # first branch's mark silently block the second from being descended
    # into at all; a set *copied* per branch -- so the guard means
    # "already on this path", not "visited anywhere" -- must still find
    # both. This is the shared-vs-copied-set decision the fix makes.
    image_a = FakeSBValue('image', 'Buffer')
    branch_a = FakeSBValue('a', 'Wrapper', children=[image_a])
    image_b = FakeSBValue('image', 'Buffer')
    branch_b = FakeSBValue('b', 'Wrapper', children=[image_b])
    root = FakeSBValue('root', 'Root', children=[branch_a, branch_b])

    found = _observable_names(root, observable_typenames={'Buffer'})

    assert found == {'root.a.image', 'root.b.image'}


# --- LldbBridge.evaluate_expression / get_available_symbols: thin wrappers
# around the shared functions above. These only prove delegation and
# LldbBridge's own result shaping (a set of names), not the traversal
# itself.

def _make_bridge(observable_typenames):
    # Bypass LldbBridge.__init__: it starts a background event-loop thread
    # and reaches into a real lldb.debugger, neither of which these tests
    # need.
    bridge = LldbBridge.__new__(LldbBridge)
    bridge._type_bridge = FakeTypeBridge(observable_typenames)
    # get_lldb_backend() reads this; its value is irrelevant once
    # frame_from_debugger is monkeypatched below.
    bridge._debugger = None
    return bridge


def test_get_available_symbols_returns_empty_set_without_a_frame(monkeypatch):
    monkeypatch.setattr(lldbbridge, 'frame_from_debugger', lambda debugger: None)

    bridge = _make_bridge(observable_typenames=set())

    assert bridge.get_available_symbols() == set()


def test_get_available_symbols_shapes_pairs_into_a_set_of_names(monkeypatch):
    monkeypatch.setattr(lldbbridge, 'frame_from_debugger', lambda debugger: object())
    monkeypatch.setattr(
        lldbbridge, 'observable_symbols',
        lambda frame, type_bridge: [('a', object()), ('a.b', object())])

    bridge = _make_bridge(observable_typenames=set())
    result = bridge.get_available_symbols()

    assert result == {'a', 'a.b'}
    assert isinstance(result, set)


def test_evaluate_expression_raises_no_stopped_frame_without_a_frame(monkeypatch):
    monkeypatch.setattr(lldbbridge, 'frame_from_debugger', lambda debugger: None)

    bridge = _make_bridge(observable_typenames=set())

    with pytest.raises(RuntimeError) as excinfo:
        bridge.evaluate_expression('x')
    assert 'no stopped frame' in str(excinfo.value)


def test_evaluate_expression_delegates_to_the_shared_evaluator_on_success(monkeypatch):
    sentinel = object()
    monkeypatch.setattr(lldbbridge, 'frame_from_debugger', lambda debugger: object())
    monkeypatch.setattr(
        lldbbridge, 'evaluate_in_frame', lambda frame, expression: sentinel)

    bridge = _make_bridge(observable_typenames=set())

    assert bridge.evaluate_expression('x') is sentinel


def test_evaluate_expression_wraps_a_non_runtime_error_from_the_shared_evaluator(monkeypatch):
    monkeypatch.setattr(lldbbridge, 'frame_from_debugger', lambda debugger: object())

    def boom(frame, expression):
        raise ValueError('kaboom')

    monkeypatch.setattr(lldbbridge, 'evaluate_in_frame', boom)

    bridge = _make_bridge(observable_typenames=set())

    with pytest.raises(RuntimeError):
        bridge.evaluate_expression('x')
