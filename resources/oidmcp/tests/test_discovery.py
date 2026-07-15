import json
import os
import sys

import pytest

from oidmcp import discovery


def _stale_and_garbage(agent_dir):
    """Drop one unparseable and one stale-but-valid discovery file."""
    (agent_dir / 'garbage.json').write_text('{not json')
    (agent_dir / '999999999.json').write_text(json.dumps({
        'version': 1, 'port': 1, 'token': 'x' * 64,
        'debugger': 'gdb', 'pid': 999999999, 'start_time': 0.0}))


def test_live_sessions_finds_running_endpoint(live_endpoint):
    session, _ = live_endpoint
    assert session.pid == os.getpid()
    assert session.debugger == 'gdb'
    assert session.port > 0
    assert len(session.token) >= 32


def test_stale_files_are_cleaned_up(tmp_path, monkeypatch):
    # A private, current-user-owned directory is a trusted OID agent dir,
    # so stale/unreadable entries are safe to unlink.
    agent_dir = tmp_path / 'agent'
    agent_dir.mkdir(mode=0o700)
    os.chmod(str(agent_dir), 0o700)
    monkeypatch.setenv('OID_AGENT_DIR', str(agent_dir))
    _stale_and_garbage(agent_dir)
    assert discovery.live_sessions() == []
    assert list(agent_dir.iterdir()) == []


@pytest.mark.skipif(sys.platform == 'win32', reason='POSIX symlinks')
def test_no_cleanup_through_symlinked_agent_dir(tmp_path, monkeypatch):
    # A symlinked OID_AGENT_DIR is not a trusted agent dir: an attacker
    # (or a footgun) could aim it at a directory whose matching *.json
    # files would then be deleted. Discovery must delete nothing there.
    real_dir = tmp_path / 'real'
    real_dir.mkdir(mode=0o700)
    os.chmod(str(real_dir), 0o700)
    _stale_and_garbage(real_dir)
    link = tmp_path / 'link'
    os.symlink(str(real_dir), str(link))
    monkeypatch.setenv('OID_AGENT_DIR', str(link))
    assert discovery.live_sessions() == []
    assert (real_dir / 'garbage.json').exists()
    assert (real_dir / '999999999.json').exists()


@pytest.mark.skipif(sys.platform == 'win32', reason='POSIX permissions')
def test_no_cleanup_in_group_or_world_accessible_dir(tmp_path, monkeypatch):
    # A group/world-accessible directory (e.g. the user accidentally
    # points OID_AGENT_DIR at ~/Documents) is not a trusted OID agent
    # dir; nothing there may be unlinked.
    agent_dir = tmp_path / 'shared'
    agent_dir.mkdir()
    # The loose 0o755 mode is the point of this test: discovery must refuse
    # to touch a dir carrying any group/other bits. Sonar S2612 flags this
    # world-accessible chmod, but it is intentional and confined to a
    # throwaway tmp_path fixture, so the finding is reviewed as safe.
    os.chmod(str(agent_dir), 0o755)  # nosec B103
    monkeypatch.setenv('OID_AGENT_DIR', str(agent_dir))
    _stale_and_garbage(agent_dir)
    assert discovery.live_sessions() == []
    assert (agent_dir / 'garbage.json').exists()
    assert (agent_dir / '999999999.json').exists()


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
