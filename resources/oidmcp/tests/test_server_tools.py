import pytest

from oidmcp import server


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


def test_plot_forwards(manager):
    mgr, _ = manager
    # FakeWindow is ready, so the plot succeeds and _plot_impl returns
    # its confirmation string naming the symbol.
    result = server._plot_impl(mgr, 'grad', None)
    assert 'grad' in result
