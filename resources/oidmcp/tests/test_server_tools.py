import os

import pytest

from oidmcp import server
from oidmcp.analysis import compute_stats
from oidmcp.protocol import ControlError


@pytest.fixture
def manager(live_endpoint):
    return server.SessionManager(), live_endpoint


def test_list_buffers(manager):
    mgr, _ = manager
    result = mgr.list_buffers(None)
    assert result['symbols'] == ['img', 'grad']


def test_fetch_decodes_and_caches(manager):
    mgr, (_, bridge) = manager
    _, arr = mgr.fetch(None, 'grad')
    assert arr.shape == (3, 4, 1)
    assert arr[2, 3, 0] == pytest.approx(23.0)
    fetches = bridge.fetch_count
    mgr.fetch(None, 'grad')          # same stop generation: cached
    assert bridge.fetch_count == fetches


def test_fetch_refetches_after_stop(manager):
    mgr, (_, bridge) = manager
    from oidscripts import agentendpoint
    mgr.fetch(None, 'grad')
    fetches = bridge.fetch_count
    agentendpoint.notify_stop()
    mgr.fetch(None, 'grad')
    assert bridge.fetch_count == fetches + 1


def test_fetch_unknown_symbol_is_friendly(manager):
    mgr, _ = manager
    with pytest.raises(RuntimeError) as excinfo:
        server._fetch_or_fail(mgr, None, 'missing')
    assert 'list_buffers' in str(excinfo.value)


def test_no_session_is_friendly(tmp_path, monkeypatch):
    monkeypatch.setenv('OID_AGENT_DIR', str(tmp_path / 'empty'))
    mgr = server.SessionManager()
    with pytest.raises(RuntimeError) as excinfo:
        server._fetch_or_fail(mgr, None, 'img')
    assert 'OID_AGENT=1' in str(excinfo.value)


def test_view_tool_returns_image_and_text(manager):
    mgr, _ = manager
    result = server._view_impl(mgr, 'grad', region=None, channel=None,
                               vmin=None, vmax=None, max_px=512,
                               session=None)
    image, text = result
    assert image.data.startswith(b'\x89PNG')
    assert 'float32' in text
    assert 'stop_generation' in text


def test_dump_tool_wraps_filesystem_errors(live_endpoint, dump_dir):
    # A path outside the dump directory becomes a friendly RuntimeError,
    # like the other tools, instead of a raw ValueError.
    escaping = str(dump_dir / 'no-such-dir' / 'out.npy')
    with pytest.raises(RuntimeError) as excinfo:
        server.dump('grad', path=escaping)
    assert 'bare filename' in str(excinfo.value)


def test_dump_tool_refuses_to_overwrite_without_flag(live_endpoint, dump_dir):
    # A filename that already holds a file is refused, so an agent
    # cannot clobber it by accident; the overwrite flag opts back in.
    first = server.dump('grad', path='out.npy')
    assert first.startswith(str(dump_dir))
    with pytest.raises(RuntimeError) as excinfo:
        server.dump('grad', path='out.npy')
    assert 'overwrite' in str(excinfo.value)
    assert server.dump('grad', path='out.npy', overwrite=True) == first


def test_plot_forwards(manager):
    mgr, _ = manager
    # FakeWindow is ready, so the plot succeeds and _plot_impl returns
    # its confirmation string naming the symbol.
    result = server._plot_impl(mgr, 'grad', None)
    assert 'grad' in result


def test_server_instructions_warn_security_and_tokens():
    instructions = server._INSTRUCTIONS
    assert 'EXPERIMENTAL' in instructions
    assert 'debuggee memory' in instructions
    assert 'Token usage' in instructions


def test_main_writes_warning_to_stderr_only(capsys, monkeypatch):
    # main() must emit the security/token warning before serving, on
    # stderr (stdout carries the stdio MCP protocol and must stay clean).
    monkeypatch.setattr(server.mcp, 'run', lambda: None)
    server.main()
    captured = capsys.readouterr()
    assert 'Security:' in captured.err
    assert 'debuggee memory' in captured.err
    assert 'Token usage:' in captured.err
    assert captured.out == ''


def test_parse_max_bytes_defaults_and_rejects_bad_values(monkeypatch):
    monkeypatch.delenv('OID_MCP_MAX_BYTES', raising=False)
    assert server._parse_max_bytes() == server.DEFAULT_MAX_BYTES
    monkeypatch.setenv('OID_MCP_MAX_BYTES', '1048576')
    assert server._parse_max_bytes() == 1048576
    for bad in ('not-a-number', '', '0', '-5'):
        monkeypatch.setenv('OID_MCP_MAX_BYTES', bad)
        assert server._parse_max_bytes() == server.DEFAULT_MAX_BYTES


def test_session_manager_survives_invalid_max_bytes(monkeypatch):
    monkeypatch.setenv('OID_MCP_MAX_BYTES', 'garbage')
    manager = server.SessionManager()  # must not raise at construction
    assert manager._max_bytes == server.DEFAULT_MAX_BYTES


# --- viewer routing (SessionManager._resolve_viewer) ------------------

def test_resolve_viewer_no_selector_picks_only_viewer(live_viewer):
    viewer = live_viewer()
    mgr = server.SessionManager()
    resolved = mgr._resolve_viewer(None)
    assert resolved.pid == viewer.pid
    assert resolved.port == viewer.port


def test_resolve_viewer_no_selector_no_viewer_is_friendly(tmp_path,
                                                          monkeypatch):
    monkeypatch.setenv('OID_AGENT_DIR', str(tmp_path / 'empty'))
    mgr = server.SessionManager()
    with pytest.raises(server.NoSessionError) as excinfo:
        mgr._resolve_viewer(None)
    assert 'OID_AGENT=1' in str(excinfo.value)


def test_resolve_viewer_by_own_pid(live_viewer):
    viewer = live_viewer(debugger_pid=None)
    mgr = server.SessionManager()
    resolved = mgr._resolve_viewer(viewer.pid)
    assert resolved.pid == viewer.pid


def test_resolve_viewer_by_debugger_pid_pairing(live_viewer):
    viewer = live_viewer(debugger_pid=424242)
    mgr = server.SessionManager()
    resolved = mgr._resolve_viewer(424242)
    assert resolved.pid == viewer.pid


def test_resolve_viewer_debugger_pid_without_viewer_raises(live_viewer):
    # One live viewer exists, but it is not paired to the requested pid:
    # this must raise a *distinct* error from the no-live-viewer-at-all
    # case, naming the specific debugger pid rather than pointing at
    # OID_AGENT=1 setup instructions.
    live_viewer(debugger_pid=None)
    mgr = server.SessionManager()
    with pytest.raises(server.NoSessionError) as excinfo:
        mgr._resolve_viewer(999999)
    assert 'viewer window' in str(excinfo.value)
    assert '999999' in str(excinfo.value)


def test_resolve_viewer_multiple_paired_picks_most_recent(live_viewer):
    live_viewer(pid=os.getpid(), debugger_pid=555, start_time=100.0)
    newest = live_viewer(pid=os.getppid(), debugger_pid=555,
                        start_time=200.0)
    mgr = server.SessionManager()
    resolved = mgr._resolve_viewer(555)
    assert resolved.pid == newest.pid
    assert resolved.start_time == pytest.approx(200.0)


# --- set_view / get_view -----------------------------------------------

def test_set_view_get_view_roundtrip(live_viewer):
    live_viewer()
    mgr = server.SessionManager()
    result = mgr.set_view(None, buffer='grad', zoom=2.0, center=[1.0, 2.0])
    assert result['buffer'] == 'grad'
    assert result['zoom'] == pytest.approx(2.0)
    assert result['center'] == [1.0, 2.0]
    # Fields not passed keep their previous value (absolute/idempotent
    # set, not a full replace): rotation_deg was never set, so it is
    # still the fixture's initial None.
    assert result['rotation_deg'] is None

    view = mgr.get_view(None)
    assert view == result


def test_set_view_only_sends_passed_fields(live_viewer):
    live_viewer()
    mgr = server.SessionManager()
    mgr.set_view(None, buffer='grad', zoom=1.0)
    second = mgr.set_view(None, rotation_deg=90)
    # zoom set earlier must survive a set_view call that doesn't mention it.
    assert second['zoom'] == pytest.approx(1.0)
    assert second['rotation_deg'] == 90


def test_set_view_tool_wraps_errors_as_runtime_error(tmp_path, monkeypatch):
    monkeypatch.setenv('OID_AGENT_DIR', str(tmp_path / 'empty'))
    with pytest.raises(RuntimeError) as excinfo:
        server.set_view(zoom=2.0)
    assert 'OID_AGENT=1' in str(excinfo.value)


def test_set_view_tool_schema_accepts_int_and_str_channel():
    # The MCP-derived schema must accept a channel as an int index (0/1/2) as
    # well as a string ("all"): the native endpoint and the docs use int
    # indices, so a str-only hint would reject the documented channel=0/1/2 at
    # the schema before the call ever reaches the viewer.
    import asyncio
    tools = asyncio.run(server.mcp.list_tools())
    schema = next(t.inputSchema for t in tools if t.name == 'set_view')
    types = {branch.get('type')
             for branch in schema['properties']['channel']['anyOf']}
    assert 'integer' in types
    assert 'string' in types


def test_get_view_tool_wraps_errors_as_runtime_error(tmp_path, monkeypatch):
    monkeypatch.setenv('OID_AGENT_DIR', str(tmp_path / 'empty'))
    with pytest.raises(RuntimeError) as excinfo:
        server.get_view()
    assert 'OID_AGENT=1' in str(excinfo.value)


# --- pixel tools falling back to a viewer session -----------------------

def test_fetch_falls_back_to_viewer_when_no_debugger_session(live_viewer):
    live_viewer()
    mgr = server.SessionManager()
    meta, arr = mgr.fetch(None, 'grad')
    assert arr.shape == (3, 4, 1)
    assert arr[2, 3, 0] == pytest.approx(23.0)
    assert meta['stop_generation'] == 0


def test_stats_against_viewer_session(live_viewer):
    live_viewer()
    mgr = server.SessionManager()
    meta, arr = mgr.fetch(None, 'grad')
    stats = compute_stats(arr, meta)
    assert stats['width'] == 4
    assert stats['height'] == 3
    assert stats['per_channel'][0]['min'] == pytest.approx(0.0)
    assert stats['per_channel'][0]['max'] == pytest.approx(23.0)


def test_list_buffers_against_viewer_session(live_viewer):
    live_viewer()
    mgr = server.SessionManager()
    result = mgr.list_buffers(None)
    assert result['symbols'] == ['grad']
    assert result['stop_generation'] == 0


def test_plot_against_viewer_session_is_a_clear_error(live_viewer):
    live_viewer()
    mgr = server.SessionManager()
    with pytest.raises(RuntimeError) as excinfo:
        mgr.plot(None, 'grad')
    assert 'already displayed' in str(excinfo.value)


def test_list_sessions_pairs_debugger_and_viewer(live_endpoint, live_viewer):
    session, _bridge = live_endpoint
    viewer = live_viewer(pid=os.getppid(), debugger_pid=session.pid)
    result = server.list_sessions()
    assert any(d['pid'] == session.pid for d in result['debuggers'])
    assert any(v['pid'] == viewer.pid and v['debugger_pid'] == session.pid
              for v in result['viewers'])


# --- connect-per-call ------------------------------------------------

def test_each_call_is_a_single_round_trip_no_ping(live_viewer):
    viewer = live_viewer()
    mgr = server.SessionManager()
    mgr.set_view(None, zoom=1.5)
    mgr.get_view(None)
    # Two calls, two wire requests: no ping precedes a warm call.
    assert viewer.endpoint.requests_served == 2


def test_control_error_reply_is_not_retried(live_viewer):
    viewer = live_viewer()
    mgr = server.SessionManager()
    with pytest.raises(ControlError):
        mgr.fetch(None, 'missing')
    # A real error reply must not be blindly re-sent.
    assert viewer.endpoint.requests_served == 1


def test_restarted_endpoint_is_reached_on_the_next_call(live_viewer):
    # A viewer that restarts between calls publishes a new port and token.
    # Connect-per-call re-resolves discovery each time, so the next call
    # reaches the new endpoint with no retry logic present.
    old = live_viewer(start_time=100.0)
    mgr = server.SessionManager()
    mgr.set_view(None, zoom=1.5)          # connects to old, closes
    old.endpoint.close()                  # old viewer "restarts"...
    new = live_viewer(start_time=200.0)   # ...republishing discovery
    view = mgr.set_view(None, zoom=2.0)   # fresh resolve + connect -> new
    assert view['zoom'] == pytest.approx(2.0)
    assert new.endpoint.requests_served == 1
    assert old.endpoint.requests_served == 1


def test_fetch_cache_does_not_bleed_across_pid_reused_debugger_sessions(
        live_endpoint):
    # A debug session that exits and a new one that starts under the same
    # OS pid (pid reuse) must not read back the previous session's cached
    # bytes, even though both sessions reach the same stop generation (a
    # freshly started endpoint's stop generation always starts at 0). The
    # cache key must therefore also discriminate on the endpoint's token,
    # which is re-randomized on every `agentendpoint.start()`.
    from conftest import FakeBridge, FakeWindow, make_meta
    from oidmcp import discovery
    from oidscripts import agentendpoint
    import numpy as np

    _, _ = live_endpoint
    mgr = server.SessionManager()
    _, arr = mgr.fetch(None, 'grad')
    assert arr[0, 0, 0] == pytest.approx(0.0)  # original gradient value

    old_pid = discovery.live_sessions()[0].pid
    old_token = discovery.live_sessions()[0].token
    agentendpoint.shutdown()

    new_raw = np.full((3, 4, 1), 42.0, dtype=np.float32)
    new_bridge = FakeBridge(
        symbols=['img', 'grad'],
        buffers={'grad': make_meta(4, 3, channels=1, type_value=5,
                                    raw=new_raw.tobytes())})
    agentendpoint.start(new_bridge, FakeWindow(ready=True))

    sessions = discovery.live_sessions()
    assert len(sessions) == 1
    assert sessions[0].pid == old_pid            # same OS pid (reused)
    assert sessions[0].token != old_token         # but a fresh token

    _, arr2 = mgr.fetch(None, 'grad')
    assert arr2[0, 0, 0] == pytest.approx(42.0)  # new session's bytes


def test_client_is_closed_when_a_call_raises(live_viewer, monkeypatch):
    live_viewer()
    mgr = server.SessionManager()
    closed = []
    real_close = server.ControlClient.close

    def spy_close(self):
        closed.append(self)
        return real_close(self)

    monkeypatch.setattr(server.ControlClient, 'close', spy_close)
    with pytest.raises(ControlError):
        mgr.fetch(None, 'missing')  # viewer get_buffer('missing') -> ControlError
    assert len(closed) == 1  # the per-call client is closed despite the error


def test_client_is_closed_after_a_successful_call(live_viewer, monkeypatch):
    live_viewer()
    mgr = server.SessionManager()
    closed = []
    real_close = server.ControlClient.close

    def spy_close(self):
        closed.append(self)
        return real_close(self)

    monkeypatch.setattr(server.ControlClient, 'close', spy_close)
    mgr.get_view(None)
    assert len(closed) == 1  # the per-call client is closed after success too


def test_client_is_closed_on_fetch_including_cache_hit(live_endpoint,
                                                        monkeypatch):
    _, bridge = live_endpoint
    mgr = server.SessionManager()
    closed = []
    real_close = server.ControlClient.close

    def spy_close(self):
        closed.append(self)
        return real_close(self)

    monkeypatch.setattr(server.ControlClient, 'close', spy_close)

    mgr.fetch(None, 'grad')
    fetches = bridge.fetch_count
    mgr.fetch(None, 'grad')  # same stop generation: served from cache

    # The cache hit still went through fetch()'s try/finally, so it never
    # re-called the bridge...
    assert bridge.fetch_count == fetches
    # ...but both calls -- the cache miss and the cache hit -- still each
    # opened and closed their own per-call client.
    assert len(closed) == 2
