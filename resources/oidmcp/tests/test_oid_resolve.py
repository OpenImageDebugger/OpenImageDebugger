"""oid_resolve: JSON resolver entry point for embedded-Python debugger hosts."""

import json
import sys

from oidscripts import oid_resolve

SENTINEL = '|OIDEND'


class FakeSymbol:
    def __init__(self, type_name):
        self.type = type_name


class FakeHost:
    """Stands in for the debugger-specific half oid_resolve delegates to."""

    def __init__(self, metadata=None, symbols=None, error=None):
        self._metadata = metadata
        self._symbols = symbols or []
        self._error = error

    def buffer_metadata(self, name):
        if self._error is not None:
            raise self._error
        return dict(self._metadata)

    def observable_symbols(self):
        return list(self._symbols)


BASE = {
    'display_name': 'm (cv::Mat)', 'width': 4, 'height': 2, 'channels': 3,
    'type': 0, 'row_stride': 4, 'pixel_layout': 'bgra',
    'transpose_buffer': False, 'pointer': 4096,
}


def _payload(text):
    assert text.endswith(SENTINEL), 'payload must carry the sentinel'
    return json.loads(text[:-len(SENTINEL)])


def test_resolve_emits_every_contract_key():
    out = _payload(oid_resolve.resolve('m', host=FakeHost(metadata=BASE)))
    assert set(out) == {
        'display_name', 'width', 'height', 'channels', 'type', 'row_stride',
        'pixel_layout', 'transpose_buffer', 'pointer', 'byte_count'}


def test_byte_count_uses_row_stride_not_width():
    """A padded buffer must be sized from row_stride; sizing from width
    under-reads it. row_stride 12 vs width 6 is a 2x under-read."""
    padded = dict(BASE, width=6, height=8, channels=1, type=5, row_stride=12)
    out = _payload(oid_resolve.resolve('m', host=FakeHost(metadata=padded)))
    assert out['byte_count'] == 4 * 1 * 12 * 8          # 384, from row_stride
    assert out['byte_count'] != 4 * 1 * 6 * 8           # 192, the wrong answer


def test_pointer_is_normalised_to_int():
    class Weird:
        def __int__(self):
            return 8192
    out = _payload(oid_resolve.resolve('m', host=FakeHost(
        metadata=dict(BASE, pointer=Weird()))))
    assert out['pointer'] == 8192
    assert isinstance(out['pointer'], int)


def test_unresolvable_symbol_reports_an_error_payload():
    out = _payload(oid_resolve.resolve('m', host=FakeHost(
        error=RuntimeError('no inspector matched'))))
    assert out['error'] == 'no inspector matched'


def test_null_pointer_is_an_error_not_a_plot():
    out = _payload(oid_resolve.resolve('m', host=FakeHost(
        metadata=dict(BASE, pointer=0))))
    assert 'error' in out


def test_metadata_missing_a_key_reports_an_error_payload():
    """A host that returns metadata missing a contract key must not let the
    KeyError from the CONTRACT_KEYS comprehension escape as a traceback."""
    incomplete = dict(BASE)
    del incomplete['row_stride']
    out = _payload(oid_resolve.resolve('m', host=FakeHost(metadata=incomplete)))
    assert 'error' in out
    assert 'm' in out['error']


def test_non_int_pointer_reports_an_error_payload():
    """A pointer that int() cannot convert must not let the
    TypeError/ValueError escape as a traceback."""
    out = _payload(oid_resolve.resolve('m', host=FakeHost(
        metadata=dict(BASE, pointer='not-a-pointer'))))
    assert 'error' in out
    assert 'm' in out['error']


def test_list_observable_paginates():
    syms = [{'name': 'v%d' % i, 'type': 'cv::Mat'} for i in range(5)]
    host = FakeHost(symbols=syms)
    first = _payload(oid_resolve.list_observable(0, 2, host=host))
    assert [s['name'] for s in first['items']] == ['v0', 'v1']
    assert first['total'] == 5
    rest = _payload(oid_resolve.list_observable(2, 99, host=host))
    assert [s['name'] for s in rest['items']] == ['v2', 'v3', 'v4']


def test_list_observable_clamps_a_negative_offset_to_the_start():
    syms = [{'name': 'v%d' % i, 'type': 'cv::Mat'} for i in range(5)]
    host = FakeHost(symbols=syms)

    out = _payload(oid_resolve.list_observable(-3, 2, host=host))

    assert out['offset'] == 0
    assert [s['name'] for s in out['items']] == ['v0', 'v1']


def test_list_observable_clamps_a_negative_limit_to_a_single_item():
    syms = [{'name': 'v%d' % i, 'type': 'cv::Mat'} for i in range(5)]
    host = FakeHost(symbols=syms)

    out = _payload(oid_resolve.list_observable(0, -7, host=host))

    assert [s['name'] for s in out['items']] == ['v0']


def test_list_observable_clamps_a_limit_above_the_maximum():
    """An unbounded limit would defeat the reason this function pages at
    all: the transports it serves truncate long strings silently, so a
    caller asking for too much must still get a bounded page, not
    everything."""
    total_symbols = oid_resolve.MAX_OBSERVABLE_PAGE_LIMIT + 20
    syms = [{'name': 'v%d' % i, 'type': 'cv::Mat'} for i in range(total_symbols)]
    host = FakeHost(symbols=syms)

    out = _payload(oid_resolve.list_observable(0, total_symbols, host=host))

    assert len(out['items']) == oid_resolve.MAX_OBSERVABLE_PAGE_LIMIT
    assert out['total'] == total_symbols


def test_list_observable_defaults_to_eight_when_no_limit_is_passed():
    """Separating the default from the ceiling must not move the default:
    a caller that asks for nothing still gets eight."""
    syms = [{'name': 'v%d' % i, 'type': 'cv::Mat'} for i in range(20)]
    host = FakeHost(symbols=syms)

    out = _payload(oid_resolve.list_observable(host=host))

    assert oid_resolve.DEFAULT_OBSERVABLE_PAGE_LIMIT == 8
    assert len(out['items']) == oid_resolve.DEFAULT_OBSERVABLE_PAGE_LIMIT


def test_list_observable_honors_a_limit_between_the_default_and_the_ceiling():
    """The whole point of separating the two roles: a caller whose own
    transport can carry more than the default must no longer be cut down
    to it, as long as it asks for no more than the ceiling."""
    limit = oid_resolve.DEFAULT_OBSERVABLE_PAGE_LIMIT + 10
    assert limit < oid_resolve.MAX_OBSERVABLE_PAGE_LIMIT
    syms = [{'name': 'v%d' % i, 'type': 'cv::Mat'} for i in range(limit + 5)]
    host = FakeHost(symbols=syms)

    out = _payload(oid_resolve.list_observable(0, limit, host=host))

    assert len(out['items']) == limit


def test_list_observable_honors_a_limit_exactly_at_the_ceiling():
    ceiling = oid_resolve.MAX_OBSERVABLE_PAGE_LIMIT
    syms = [{'name': 'v%d' % i, 'type': 'cv::Mat'} for i in range(ceiling + 5)]
    host = FakeHost(symbols=syms)

    out = _payload(oid_resolve.list_observable(0, ceiling, host=host))

    assert len(out['items']) == ceiling


def test_list_observable_clamps_a_limit_one_above_the_ceiling():
    ceiling = oid_resolve.MAX_OBSERVABLE_PAGE_LIMIT
    syms = [{'name': 'v%d' % i, 'type': 'cv::Mat'} for i in range(ceiling + 5)]
    host = FakeHost(symbols=syms)

    out = _payload(oid_resolve.list_observable(0, ceiling + 1, host=host))

    assert len(out['items']) == ceiling


def test_list_observable_total_reports_every_symbol_regardless_of_page_size():
    """total must reflect the true symbol count whether the caller takes
    a small page, the default, or the whole list in one go."""
    total_symbols = oid_resolve.MAX_OBSERVABLE_PAGE_LIMIT + 37
    syms = [{'name': 'v%d' % i, 'type': 'cv::Mat'}
            for i in range(total_symbols)]
    host = FakeHost(symbols=syms)

    small_page = _payload(oid_resolve.list_observable(0, 3, host=host))
    default_page = _payload(oid_resolve.list_observable(host=host))
    full_page = _payload(
        oid_resolve.list_observable(0, total_symbols, host=host))

    assert small_page['total'] == total_symbols
    assert default_page['total'] == total_symbols
    assert full_page['total'] == total_symbols


def test_payloads_always_carry_the_sentinel():
    assert oid_resolve.resolve('m', host=FakeHost(metadata=BASE)).endswith(SENTINEL)
    assert oid_resolve.list_observable(0, 1, host=FakeHost()).endswith(SENTINEL)


# --- C1: host acquisition on the default (no explicit host) path must be
# guarded too, since current_host() raises RuntimeError for entirely
# routine conditions (no stopped frame, no debugger at all). This is the
# primary calling mode real callers use.

def test_resolve_with_no_host_and_no_debugger_returns_error_not_traceback(monkeypatch):
    monkeypatch.delitem(sys.modules, 'lldb', raising=False)
    monkeypatch.delitem(sys.modules, 'gdb', raising=False)

    result = oid_resolve.resolve('m')

    assert result.endswith(SENTINEL), \
        'a raised RuntimeError from host acquisition must not escape as a traceback'
    assert 'error' in _payload(result)


def test_list_observable_with_no_host_and_no_debugger_returns_error_not_traceback(monkeypatch):
    monkeypatch.delitem(sys.modules, 'lldb', raising=False)
    monkeypatch.delitem(sys.modules, 'gdb', raising=False)

    result = oid_resolve.list_observable()

    assert result.endswith(SENTINEL), \
        'a raised RuntimeError from host acquisition must not escape as a traceback'
    assert 'error' in _payload(result)


# --- I3: emission itself must stay inside the guard. A host is free to
# hand back a contract value of any type (that is why `pointer` is
# normalised elsewhere); one that is not JSON-serializable must still
# yield an error payload rather than a bare TypeError traceback.

class _Unserializable:
    """A value json.dumps cannot encode, standing in for a debugger-native
    object a host forgot to normalise before returning it."""


def test_resolve_with_unserializable_contract_value_reports_error_not_traceback():
    bad_metadata = dict(BASE, pixel_layout=_Unserializable())

    result = oid_resolve.resolve('m', host=FakeHost(metadata=bad_metadata))

    assert result.endswith(SENTINEL)
    assert 'error' in _payload(result)


def test_list_observable_with_unserializable_symbol_reports_error_not_traceback():
    host = FakeHost(symbols=[{'name': 'v', 'type': _Unserializable()}])

    result = oid_resolve.list_observable(host=host)

    assert result.endswith(SENTINEL)
    assert 'error' in _payload(result)
