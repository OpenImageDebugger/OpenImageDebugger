import pytest

from oidscripts import agentendpoint as ep
from conftest import FakeBridge, FakeWindow, make_meta


def make_endpoint(bridge=None, window=None, **kwargs):
    return ep.AgentEndpoint(bridge or FakeBridge(),
                            window or FakeWindow(),
                            token='sekret', **kwargs)


def test_hello_reports_session_facts():
    endpoint = make_endpoint(bridge=FakeBridge(backend='lldb'))
    response, payload = endpoint.handle_request(
        {'method': 'hello', 'token': 'sekret'})
    assert response['version'] == ep.PROTOCOL_VERSION
    assert response['debugger'] == 'lldb'
    assert response['stop_generation'] == 0
    assert isinstance(response['pid'], int)
    assert payload == b''


def test_hello_with_bad_token_is_rejected():
    endpoint = make_endpoint()
    with pytest.raises(ep.EndpointError) as excinfo:
        endpoint.handle_request({'method': 'hello', 'token': 'wrong'})
    assert excinfo.value.code == ep.ERROR_BAD_TOKEN


def test_unknown_method():
    endpoint = make_endpoint()
    with pytest.raises(ep.EndpointError) as excinfo:
        endpoint.handle_request({'method': 'frobnicate'})
    assert excinfo.value.code == ep.ERROR_UNKNOWN_METHOD


def test_non_string_method_is_unknown_method():
    # An unhashable method (e.g. a list) must map to a protocol error,
    # not a TypeError that would kill the client thread.
    endpoint = make_endpoint()
    with pytest.raises(ep.EndpointError) as excinfo:
        endpoint.handle_request({'method': ['x']})
    assert excinfo.value.code == ep.ERROR_UNKNOWN_METHOD


def test_ping_tracks_stop_generation():
    endpoint = make_endpoint()
    endpoint.notify_stop()
    endpoint.notify_stop()
    response, _ = endpoint.handle_request({'method': 'ping'})
    assert response == {'stop_generation': 2}


def test_list_symbols():
    endpoint = make_endpoint(bridge=FakeBridge(symbols=['img', 'mat']))
    response, _ = endpoint.handle_request({'method': 'list_symbols'})
    assert response['symbols'] == ['img', 'mat']
    assert response['stop_generation'] == 0


def test_get_buffer_returns_metadata_and_pixels():
    raw = bytes(range(48))  # 4x3 uint8, 4 channels
    meta = make_meta(4, 3, channels=4, type_value=0, raw=raw)
    endpoint = make_endpoint(bridge=FakeBridge(buffers={'img': meta}))
    response, payload = endpoint.handle_request(
        {'method': 'get_buffer', 'symbol': 'img'})
    assert payload == raw
    assert response['width'] == 4
    assert response['height'] == 3
    assert response['channels'] == 4
    assert response['type'] == 0
    assert response['stop_generation'] == 0
    assert 'pointer' not in response


def test_get_buffer_unknown_symbol():
    endpoint = make_endpoint()
    with pytest.raises(ep.EndpointError) as excinfo:
        endpoint.handle_request({'method': 'get_buffer', 'symbol': 'nope'})
    assert excinfo.value.code == ep.ERROR_SYMBOL_NOT_FOUND


def test_get_buffer_too_large():
    meta = make_meta(100, 100, type_value=0)
    endpoint = make_endpoint(bridge=FakeBridge(buffers={'img': meta}))
    with pytest.raises(ep.EndpointError) as excinfo:
        endpoint.handle_request(
            {'method': 'get_buffer', 'symbol': 'img', 'max_bytes': 64})
    assert excinfo.value.code == ep.ERROR_TOO_LARGE


def test_get_buffer_while_running_maps_to_not_stopped():
    bridge = FakeBridge(error=RuntimeError('Selected thread is running.'))
    endpoint = make_endpoint(bridge=bridge)
    with pytest.raises(ep.EndpointError) as excinfo:
        endpoint.handle_request({'method': 'get_buffer', 'symbol': 'img'})
    assert excinfo.value.code == ep.ERROR_NOT_STOPPED


def test_get_buffer_other_errors_map_to_internal():
    bridge = FakeBridge(error=ValueError('Invalid buffer of zero bytes'))
    endpoint = make_endpoint(bridge=bridge)
    with pytest.raises(ep.EndpointError) as excinfo:
        endpoint.handle_request({'method': 'get_buffer', 'symbol': 'img'})
    assert excinfo.value.code == ep.ERROR_INTERNAL
    assert 'zero bytes' in excinfo.value.message


def test_unanswered_request_times_out():
    endpoint = make_endpoint(bridge=FakeBridge(dead=True),
                             request_timeout=0.05)
    with pytest.raises(ep.EndpointError) as excinfo:
        endpoint.handle_request({'method': 'list_symbols'})
    assert excinfo.value.code == ep.ERROR_TIMEOUT


def test_plot_forwards_to_ready_window():
    window = FakeWindow(ready=True)
    endpoint = make_endpoint(window=window)
    response, _ = endpoint.handle_request({'method': 'plot', 'symbol': 'img'})
    assert response == {'ok': True}
    assert window.plotted == ['img']


def test_plot_without_viewer():
    endpoint = make_endpoint(window=FakeWindow(ready=False))
    with pytest.raises(ep.EndpointError) as excinfo:
        endpoint.handle_request({'method': 'plot', 'symbol': 'img'})
    assert excinfo.value.code == ep.ERROR_NO_VIEWER


def test_plot_failure_maps_to_internal():
    endpoint = make_endpoint(window=FakeWindow(ready=True, plot_result=0))
    with pytest.raises(ep.EndpointError) as excinfo:
        endpoint.handle_request({'method': 'plot', 'symbol': 'img'})
    assert excinfo.value.code == ep.ERROR_INTERNAL


def test_get_buffer_passes_memoryview_pointer_without_copy():
    # A bridge that hands back a memoryview (as gdb's read_memory does) must
    # be forwarded without an eager bytes() copy, yet still yield the exact
    # bytes on the wire.
    raw = bytes(range(48))
    meta = make_meta(4, 3, channels=4, type_value=0, raw=raw)
    meta['pointer'] = memoryview(bytearray(raw))
    endpoint = make_endpoint(bridge=FakeBridge(buffers={'img': meta}))
    response, payload = endpoint.handle_request(
        {'method': 'get_buffer', 'symbol': 'img'})
    assert isinstance(payload, (bytes, bytearray, memoryview))
    assert bytes(payload) == raw
    assert response['width'] == 4
