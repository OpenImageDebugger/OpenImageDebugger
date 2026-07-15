import json
import os

import pytest

from oidmcp import discovery


def test_live_sessions_finds_running_endpoint(live_endpoint):
    session, _ = live_endpoint
    assert session.pid == os.getpid()
    assert session.debugger == 'gdb'
    assert session.port > 0
    assert len(session.token) >= 32


def test_stale_files_are_cleaned_up(tmp_path, monkeypatch):
    agent_dir = tmp_path / 'agent'
    agent_dir.mkdir()
    monkeypatch.setenv('OID_AGENT_DIR', str(agent_dir))
    (agent_dir / '999999999.json').write_text(json.dumps({
        'version': 1, 'port': 1, 'token': 'x' * 64,
        'debugger': 'gdb', 'pid': 999999999, 'start_time': 0.0}))
    (agent_dir / 'garbage.json').write_text('{not json')
    assert discovery.live_sessions() == []
    assert list(agent_dir.iterdir()) == []


def test_pick_session_default_is_most_recent(tmp_path, monkeypatch):
    monkeypatch.setenv('OID_AGENT_DIR', str(tmp_path))
    old = discovery.SessionInfo(path=tmp_path / 'a.json', pid=1, port=1,
                                token='t', debugger='gdb', start_time=1.0)
    new = discovery.SessionInfo(path=tmp_path / 'b.json', pid=2, port=2,
                                token='t', debugger='lldb', start_time=2.0)
    assert discovery.pick_session([old, new]) is new
    assert discovery.pick_session([old, new], session=1) is old


def test_pick_session_errors_are_actionable():
    with pytest.raises(discovery.NoSessionError) as excinfo:
        discovery.pick_session([])
    assert 'OID_AGENT=1' in str(excinfo.value)
    sessions = [discovery.SessionInfo(path=None, pid=1, port=1, token='t',
                                      debugger='gdb', start_time=1.0)]
    with pytest.raises(discovery.NoSessionError):
        discovery.pick_session(sessions, session=42)
