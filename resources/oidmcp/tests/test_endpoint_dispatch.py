import queue
import threading
import time

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
    # A bridge may hand back a memoryview (gdb's read_memory does); it must
    # reach the wire without an eager bytes() copy.
    raw = bytes(range(48))
    meta = make_meta(4, 3, channels=4, type_value=0, raw=raw)
    meta['pointer'] = memoryview(bytearray(raw))
    endpoint = make_endpoint(bridge=FakeBridge(buffers={'img': meta}))
    response, payload = endpoint.handle_request(
        {'method': 'get_buffer', 'symbol': 'img'})
    assert isinstance(payload, (bytes, bytearray, memoryview))
    assert bytes(payload) == raw
    assert response['width'] == 4


def test_get_buffer_enforces_endpoint_cap_when_client_omits(monkeypatch):
    # Omitting max_bytes must NOT disable the guard: the endpoint applies its
    # own ceiling, so an oversized buffer is still rejected.
    monkeypatch.setattr(ep, 'ENDPOINT_MAX_BYTES', 16)
    meta = make_meta(10, 10, type_value=0)  # 100 bytes > 16
    endpoint = make_endpoint(bridge=FakeBridge(buffers={'img': meta}))
    with pytest.raises(ep.EndpointError) as excinfo:
        endpoint.handle_request({'method': 'get_buffer', 'symbol': 'img'})
    assert excinfo.value.code == ep.ERROR_TOO_LARGE


def test_get_buffer_clamps_client_cap_to_endpoint_max(monkeypatch):
    # A client cannot raise the cap above the endpoint ceiling.
    monkeypatch.setattr(ep, 'ENDPOINT_MAX_BYTES', 16)
    meta = make_meta(10, 10, type_value=0)  # 100 bytes
    endpoint = make_endpoint(bridge=FakeBridge(buffers={'img': meta}))
    with pytest.raises(ep.EndpointError) as excinfo:
        endpoint.handle_request(
            {'method': 'get_buffer', 'symbol': 'img', 'max_bytes': 10 ** 9})
    assert excinfo.value.code == ep.ERROR_TOO_LARGE


def test_get_buffer_honors_smaller_client_cap(monkeypatch):
    monkeypatch.setattr(ep, 'ENDPOINT_MAX_BYTES', 10 ** 6)
    raw = bytes(48)
    meta = make_meta(4, 3, channels=4, type_value=0, raw=raw)  # 48 bytes
    endpoint = make_endpoint(bridge=FakeBridge(buffers={'img': meta}))
    _, payload = endpoint.handle_request(
        {'method': 'get_buffer', 'symbol': 'img', 'max_bytes': 48})
    assert bytes(payload) == raw


class SerialBridge(FakeBridge):
    # One drain thread runs queued callables in order, like the debugger.

    def __init__(self, delay=0.0, **kwargs):
        super().__init__(**kwargs)
        self.delay = delay
        self._queue = queue.Queue()
        threading.Thread(target=self._drain, daemon=True).start()

    def queue_request(self, callable_request):
        self._queue.put(callable_request)

    def _drain(self):
        while True:
            self._queue.get()()

    def get_buffer_metadata(self, variable, max_bytes=None):
        time.sleep(self.delay)
        return super().get_buffer_metadata(variable, max_bytes=max_bytes)


def _call_in_threads(endpoint, request, count, join_timeout):
    results = [None] * count

    def run(index):
        try:
            results[index] = endpoint.handle_request(dict(request))
        except ep.EndpointError as error:
            results[index] = error

    threads = [threading.Thread(target=run, args=(i,), daemon=True)
               for i in range(count)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join(join_timeout)
    assert not any(thread.is_alive() for thread in threads), 'hung'
    return results


def test_each_queued_request_gets_the_full_timeout():
    # The last starts at ~0.7 s, past its 0.5 s timeout; progress keeps it.
    bridge = SerialBridge(delay=0.1, symbols=['grad'],
                          buffers={'grad': make_meta(4, 3, type_value=0)})
    endpoint = make_endpoint(bridge=bridge, request_timeout=0.5)
    results = _call_in_threads(
        endpoint, {'method': 'get_buffer', 'symbol': 'grad'}, 8, 10)
    assert not any(isinstance(r, ep.EndpointError) for r in results)
    assert all(r[0]['width'] == 4 for r in results)


def test_a_request_after_an_idle_pause_still_runs():
    # Idle past the timeout, then the first request must not expire at once.
    bridge = SerialBridge(delay=0.05, symbols=['grad'],
                          buffers={'grad': make_meta(4, 3, type_value=0)})
    endpoint = make_endpoint(bridge=bridge, request_timeout=0.2)
    time.sleep(0.3)
    [result] = _call_in_threads(
        endpoint, {'method': 'get_buffer', 'symbol': 'grad'}, 1, 5)
    assert result[0]['width'] == 4


def test_a_hung_debugger_still_times_out_queued_requests():
    # MAX_CLIENTS x timeout (2.4 s) overruns the window: progress-blind fails.
    timeout = 0.3
    window = 2 * timeout + 0.5
    endpoint = make_endpoint(bridge=FakeBridge(dead=True),
                             request_timeout=timeout)
    started = time.monotonic()
    results = _call_in_threads(
        endpoint, {'method': 'list_symbols'}, 3, window)
    assert time.monotonic() - started < window
    assert [r.code for r in results] == [ep.ERROR_TIMEOUT] * 3


def test_a_task_started_after_its_caller_gave_up_does_nothing():
    bridge = FakeBridge(dead=True,
                        buffers={'img': make_meta(4, 3, type_value=0)})
    endpoint = make_endpoint(bridge=bridge, request_timeout=0.05)
    [result] = _call_in_threads(
        endpoint, {'method': 'get_buffer', 'symbol': 'img'}, 1, 5)
    assert result.code == ep.ERROR_TIMEOUT
    for late in bridge.pending:
        late()
    assert bridge.fetch_count == 0


def test_a_started_request_that_hangs_still_times_out():
    release = threading.Event()

    class HangingBridge(SerialBridge):
        def get_buffer_metadata(self, variable, max_bytes=None):
            release.wait()
            return super().get_buffer_metadata(variable, max_bytes)

    endpoint = make_endpoint(bridge=HangingBridge(), request_timeout=0.3)
    results = []

    def run():
        try:
            endpoint.handle_request(
                {'method': 'get_buffer', 'symbol': 'img'})
        except ep.EndpointError as error:
            results.append(error)

    thread = threading.Thread(target=run, daemon=True)
    thread.start()
    thread.join(3)
    release.set()
    assert not thread.is_alive(), 'hung'
    assert [r.code for r in results] == [ep.ERROR_TIMEOUT]
