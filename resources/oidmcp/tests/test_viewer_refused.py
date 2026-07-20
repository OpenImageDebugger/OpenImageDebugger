"""A live-pid viewer discovery file whose port refuses is 'dead, skip'."""

import os

import pytest

from oidmcp import server as srv
from oidmcp.discovery import NoSessionError, ViewerSessionInfo


class _FakeClient:
    def __init__(self):
        self.closed = False

    def close(self):
        self.closed = True


def _viewer(path, port, start_time):
    return ViewerSessionInfo(path=path, pid=os.getpid(), port=port,
                             token='t' * 64, start_time=start_time,
                             debugger_pid=None)


def test_refused_viewer_is_reaped_and_next_one_serves(monkeypatch, tmp_path):
    stale_path = tmp_path / '101.json'
    good_path = tmp_path / '102.json'
    stale_path.write_text('{}')
    good_path.write_text('{}')
    # The stale entry is newer, so selection tries it first.
    stale = _viewer(stale_path, port=1, start_time=2.0)
    good = _viewer(good_path, port=2, start_time=1.0)

    def fake_live_viewers():
        return [v for v in (stale, good) if v.path.exists()]

    monkeypatch.setattr(srv, 'live_viewers', fake_live_viewers)

    client = _FakeClient()

    def fake_connect(self, info):
        if info.port == 1:
            raise ConnectionRefusedError()
        return client

    monkeypatch.setattr(srv.SessionManager, '_connect', fake_connect)

    result = srv.SessionManager()._call_viewer(None, lambda c: 'served')

    assert result == 'served'
    assert not stale_path.exists()   # refused entry reaped
    assert good_path.exists()        # serving entry untouched
    assert client.closed


def test_all_viewers_refused_raises_no_session(monkeypatch, tmp_path):
    path = tmp_path / '103.json'
    path.write_text('{}')
    only = _viewer(path, port=1, start_time=1.0)

    def fake_live_viewers():
        return [only] if path.exists() else []

    monkeypatch.setattr(srv, 'live_viewers', fake_live_viewers)

    def fake_connect(self, info):
        raise ConnectionRefusedError()

    monkeypatch.setattr(srv.SessionManager, '_connect', fake_connect)

    mgr = srv.SessionManager()
    with pytest.raises(NoSessionError):
        mgr._call_viewer(None, lambda c: 'never')
    assert not path.exists()


def test_many_stale_viewers_do_not_hide_an_older_live_one(monkeypatch, tmp_path):
    # Nine newer stale entries that all refuse, plus one older live viewer.
    # A fixed small retry cap would give up before reaching the live one; the
    # live-viewer-bounded walk must reap all nine and still serve the tenth.
    stale = []
    for i in range(9):
        p = tmp_path / f'2{i:02d}.json'
        p.write_text('{}')
        stale.append(_viewer(p, port=1, start_time=100.0 + i))
    good_path = tmp_path / '100.json'
    good_path.write_text('{}')
    good = _viewer(good_path, port=2, start_time=1.0)  # oldest -> tried last

    def fake_live_viewers():
        return [v for v in (*stale, good) if v.path.exists()]

    monkeypatch.setattr(srv, 'live_viewers', fake_live_viewers)

    client = _FakeClient()

    def fake_connect(self, info):
        if info.port == 2:
            return client
        raise ConnectionRefusedError()

    monkeypatch.setattr(srv.SessionManager, '_connect', fake_connect)

    result = srv.SessionManager()._call_viewer(None, lambda c: 'served')

    assert result == 'served'
    assert good_path.exists()                        # live entry untouched
    assert all(not v.path.exists() for v in stale)   # every stale entry reaped
    assert client.closed


def test_unreapable_refused_viewer_does_not_loop_forever(monkeypatch, tmp_path):
    # If a refused entry cannot be reaped (its file survives _reap), the walk
    # must still terminate rather than spin forever on the same entry.
    path = tmp_path / '300.json'
    path.write_text('{}')
    only = _viewer(path, port=1, start_time=1.0)

    monkeypatch.setattr(srv, 'live_viewers', lambda: [only])  # never disappears
    monkeypatch.setattr(srv, '_reap', lambda p: None)         # reap is a no-op

    def fake_connect(self, info):
        raise ConnectionRefusedError()

    monkeypatch.setattr(srv.SessionManager, '_connect', fake_connect)

    mgr = srv.SessionManager()
    with pytest.raises(NoSessionError):
        mgr._call_viewer(None, lambda c: 'never')
