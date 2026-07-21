import json
import os
import sys
import tempfile
from pathlib import Path

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
def test_untrusted_dir_is_neither_read_nor_cleaned(tmp_path, monkeypatch):
    # A group/world-accessible directory (e.g. the user accidentally points
    # OID_AGENT_DIR at ~/Documents) is not a trusted OID agent dir: a token
    # there could have been read or planted by another user. Discovery must
    # surface no session from it -- not even a well-formed, live one -- and
    # must unlink nothing there.
    agent_dir = tmp_path / 'shared'
    agent_dir.mkdir()
    # The loose 0o755 mode is the point of this test: discovery must refuse
    # to touch a dir carrying any group/other bits. Sonar S2612 flags this
    # world-accessible chmod, but it is intentional and confined to a
    # throwaway tmp_path fixture, so the finding is reviewed as safe.
    os.chmod(str(agent_dir), 0o755)  # nosec B103
    monkeypatch.setenv('OID_AGENT_DIR', str(agent_dir))
    _stale_and_garbage(agent_dir)
    # A live, valid session for this very process would be usable in a
    # trusted dir; here it must still be refused.
    live = agent_dir / f'{os.getpid()}.json'
    live.write_text(json.dumps({
        'version': 1, 'port': 5555, 'token': 'x' * 64,
        'debugger': 'gdb', 'pid': os.getpid(), 'start_time': 1.0}))
    assert discovery.live_sessions() == []
    assert (agent_dir / 'garbage.json').exists()
    assert (agent_dir / '999999999.json').exists()
    assert live.exists()


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


def _make_agent_dir(tmp_path, monkeypatch):
    agent_dir = tmp_path / 'agent'
    agent_dir.mkdir(mode=0o700)
    os.chmod(str(agent_dir), 0o700)
    monkeypatch.setenv('OID_AGENT_DIR', str(agent_dir))
    return agent_dir


def test_live_viewers_finds_running_endpoint(tmp_path, monkeypatch):
    agent_dir = _make_agent_dir(tmp_path, monkeypatch)
    viewer_dir = agent_dir / 'viewer'
    viewer_dir.mkdir(mode=0o700)
    os.chmod(str(viewer_dir), 0o700)
    assert discovery.viewer_discovery_dir() == viewer_dir
    (viewer_dir / '1234.json').write_text(json.dumps({
        'version': 1, 'port': 6001, 'token': 'y' * 64,
        'pid': os.getpid(), 'start_time': 5.0, 'debugger_pid': 42}))

    viewers = discovery.live_viewers()

    assert len(viewers) == 1
    viewer = viewers[0]
    assert viewer.path == viewer_dir / '1234.json'
    assert viewer.pid == os.getpid()
    assert viewer.port == 6001
    assert viewer.token == 'y' * 64
    assert viewer.start_time == pytest.approx(5.0)
    assert viewer.debugger_pid == 42


def test_live_viewers_reaps_malformed_and_keeps_valid(tmp_path, monkeypatch):
    # A discovery file whose debugger_pid is a JSON object makes int() raise
    # TypeError in _parse_viewer; enumeration must reap that file and still
    # return the valid entries rather than aborting on the malformed one.
    agent_dir = _make_agent_dir(tmp_path, monkeypatch)
    viewer_dir = agent_dir / 'viewer'
    viewer_dir.mkdir(mode=0o700)
    os.chmod(str(viewer_dir), 0o700)
    good = viewer_dir / 'good.json'
    good.write_text(json.dumps({
        'version': 1, 'port': 6002, 'token': 'z' * 64,
        'pid': os.getpid(), 'start_time': 1.0, 'debugger_pid': 7}))
    bad = viewer_dir / 'bad.json'
    bad.write_text(json.dumps({
        'version': 1, 'port': 6003, 'token': 'z' * 64,
        'pid': os.getpid(), 'start_time': 2.0, 'debugger_pid': {}}))

    viewers = discovery.live_viewers()

    assert [v.port for v in viewers] == [6002]
    assert not bad.exists()  # malformed file reaped
    assert good.exists()     # valid entry untouched


def test_live_viewers_survives_directory_entry(tmp_path, monkeypatch):
    # A *.json entry that is itself a directory cannot be read (OSError) or
    # unlinked; _reap must swallow the unlink failure and enumeration must
    # still return the valid entries instead of crashing.
    agent_dir = _make_agent_dir(tmp_path, monkeypatch)
    viewer_dir = agent_dir / 'viewer'
    viewer_dir.mkdir(mode=0o700)
    os.chmod(str(viewer_dir), 0o700)
    (viewer_dir / 'good.json').write_text(json.dumps({
        'version': 1, 'port': 6004, 'token': 'z' * 64,
        'pid': os.getpid(), 'start_time': 1.0, 'debugger_pid': 8}))
    (viewer_dir / 'adir.json').mkdir()

    viewers = discovery.live_viewers()

    assert [v.port for v in viewers] == [6004]
    assert (viewer_dir / 'adir.json').exists()  # unremovable dir left intact


def test_live_viewers_reaps_nonpositive_pid(tmp_path, monkeypatch):
    # A record with pid <= 0 is malformed; os.kill(0/neg, 0) has special
    # process-group/broadcast semantics that would look "alive", so the entry
    # must be treated as dead and reaped rather than routed to.
    agent_dir = _make_agent_dir(tmp_path, monkeypatch)
    viewer_dir = agent_dir / 'viewer'
    viewer_dir.mkdir(mode=0o700)
    os.chmod(str(viewer_dir), 0o700)
    bad = viewer_dir / 'zeropid.json'
    bad.write_text(json.dumps({
        'version': 1, 'port': 6005, 'token': 'z' * 64,
        'pid': 0, 'start_time': 1.0, 'debugger_pid': 9}))

    viewers = discovery.live_viewers()

    assert viewers == []
    assert not bad.exists()  # reaped


def test_viewer_and_debugger_sessions_do_not_cross(tmp_path, monkeypatch):
    agent_dir = _make_agent_dir(tmp_path, monkeypatch)
    (agent_dir / f'{os.getpid()}.json').write_text(json.dumps({
        'version': 1, 'port': 1, 'token': 'x' * 64,
        'debugger': 'gdb', 'pid': os.getpid(), 'start_time': 1.0}))
    viewer_dir = agent_dir / 'viewer'
    viewer_dir.mkdir(mode=0o700)
    os.chmod(str(viewer_dir), 0o700)
    (viewer_dir / f'{os.getpid()}.json').write_text(json.dumps({
        'version': 1, 'port': 2, 'token': 'y' * 64,
        'pid': os.getpid(), 'start_time': 2.0}))

    sessions = discovery.live_sessions()
    assert len(sessions) == 1
    assert sessions[0].port == 1

    viewers = discovery.live_viewers()
    assert len(viewers) == 1
    assert viewers[0].port == 2


@pytest.mark.skipif(sys.platform == 'win32', reason='POSIX permissions')
def test_untrusted_viewer_dir_is_refused(tmp_path, monkeypatch, capsys):
    agent_dir = _make_agent_dir(tmp_path, monkeypatch)
    viewer_dir = agent_dir / 'viewer'
    viewer_dir.mkdir()
    # See test_untrusted_dir_is_neither_read_nor_cleaned: the loose mode
    # is the point of this test. nosec B103
    os.chmod(str(viewer_dir), 0o777)  # nosec B103  # NOSONAR
    live = viewer_dir / f'{os.getpid()}.json'
    live.write_text(json.dumps({
        'version': 1, 'port': 5555, 'token': 'x' * 64,
        'pid': os.getpid(), 'start_time': 1.0}))

    assert discovery.live_viewers() == []
    assert live.exists()
    err = capsys.readouterr().err
    assert err.count('Refusing to read OID discovery files') == 1


def test_live_viewers_debugger_pid_optional(tmp_path, monkeypatch):
    agent_dir = _make_agent_dir(tmp_path, monkeypatch)
    viewer_dir = agent_dir / 'viewer'
    viewer_dir.mkdir(mode=0o700)
    os.chmod(str(viewer_dir), 0o700)
    (viewer_dir / 'with_dbg.json').write_text(json.dumps({
        'version': 1, 'port': 1, 'token': 'a' * 64,
        'pid': os.getpid(), 'start_time': 1.0, 'debugger_pid': 77}))
    (viewer_dir / 'without_dbg.json').write_text(json.dumps({
        'version': 1, 'port': 2, 'token': 'b' * 64,
        'pid': os.getpid(), 'start_time': 2.0}))

    viewers = {v.path.name: v for v in discovery.live_viewers()}

    assert viewers['with_dbg.json'].debugger_pid == 77
    assert viewers['without_dbg.json'].debugger_pid is None


def test_pid_alive_never_calls_os_kill_on_windows(monkeypatch):
    # os.kill(pid, 0) is destructive on Windows (CTRL_C_EVENT /
    # TerminateProcess), so liveness must route through the process-handle
    # probe and never touch os.kill -- otherwise enumerating discovery files
    # would kill the viewers/debuggers it finds.
    monkeypatch.setattr(os, 'name', 'nt')
    called = []
    monkeypatch.setattr(os, 'kill', lambda *args: called.append(args))
    monkeypatch.setattr(discovery, '_pid_alive_windows', lambda pid: True)

    assert discovery._pid_alive(4321) is True
    assert called == []


def test_pid_alive_uses_os_kill_on_posix(monkeypatch):
    monkeypatch.setattr(os, 'name', 'posix')
    seen = []
    monkeypatch.setattr(os, 'kill', lambda pid, sig: seen.append((pid, sig)))

    assert discovery._pid_alive(4321) is True
    assert seen == [(4321, 0)]


def test_pid_alive_nonpositive_is_dead_without_probing(monkeypatch):
    # A non-positive pid is dead on every platform without invoking either
    # probe (os.kill's special group/broadcast semantics or the Windows path).
    monkeypatch.setattr(os, 'name', 'nt')
    monkeypatch.setattr(
        discovery, '_pid_alive_windows',
        lambda pid: pytest.fail('non-positive pid must not be probed'))
    monkeypatch.setattr(
        os, 'kill',
        lambda *args: pytest.fail('non-positive pid must not be probed'))

    assert discovery._pid_alive(0) is False
    assert discovery._pid_alive(-1) is False


@pytest.mark.skipif(sys.platform == 'win32', reason='POSIX passwd home')
def test_discovery_dir_is_home_based_and_env_independent(monkeypatch):
    # The dir must come from the passwd database, not $HOME/$TMPDIR/$XDG_*,
    # so a stripped-env MCP subprocess and the GUI viewer agree. Skip where
    # the uid has no passwd entry: _home_dir() then falls back to $HOME, which
    # this env-independence assertion cannot hold for.
    import pwd
    try:
        home = pwd.getpwuid(os.getuid()).pw_dir
    except (KeyError, OSError):
        pytest.skip('no passwd entry for this uid')
    if not home:
        pytest.skip('empty passwd home for this uid')
    monkeypatch.delenv('OID_AGENT_DIR', raising=False)
    for var in ('TMPDIR', 'HOME', 'XDG_RUNTIME_DIR', 'XDG_CACHE_HOME',
                'USER', 'LOGNAME'):
        monkeypatch.delenv(var, raising=False)
    assert discovery.discovery_dir() == Path(home) / '.oid-agent'
    # Skewing TMPDIR/HOME must not move it (passwd-based, not env-based).
    monkeypatch.setenv('TMPDIR', '/env/ignored-tmpdir')
    monkeypatch.setenv('HOME', '/env/ignored-home')
    assert discovery.discovery_dir() == Path(home) / '.oid-agent'


def test_discovery_dir_respects_override(monkeypatch):
    monkeypatch.setenv('OID_AGENT_DIR', '/custom/base')
    assert discovery.discovery_dir() == Path('/custom/base')


@pytest.mark.skipif(sys.platform == 'win32', reason='POSIX temp fallback')
def test_discovery_dir_temp_fallback_is_per_uid(monkeypatch):
    # No passwd entry and no $HOME: the fallback must be a per-uid temp dir so
    # different uids do not contend for one shared /tmp/.oid-agent (which the
    # owner check would then refuse).
    import pwd

    def _no_passwd(_uid):
        raise KeyError('no passwd entry')

    monkeypatch.delenv('OID_AGENT_DIR', raising=False)
    monkeypatch.delenv('HOME', raising=False)
    monkeypatch.setattr(pwd, 'getpwuid', _no_passwd)
    expected = Path(tempfile.gettempdir()) / f'oid-agent-{os.getuid()}'
    assert discovery.discovery_dir() == expected
