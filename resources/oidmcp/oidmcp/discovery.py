"""Locate live OID debug sessions via their discovery files."""

from __future__ import annotations

import json
import os
import stat
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


class NoSessionError(Exception):
    pass


@dataclass(frozen=True)
class SessionInfo:
    path: Path | None
    pid: int
    port: int
    token: str
    debugger: str
    start_time: float


@dataclass(frozen=True)
class ViewerSessionInfo:
    path: Path | None
    pid: int
    port: int
    token: str
    start_time: float
    debugger_pid: int | None


def _home_dir():
    # Passwd-based home so a stripped-env MCP subprocess and the GUI-launched
    # viewer resolve the same dir independent of $HOME/$TMPDIR/$XDG_*. Returns
    # None when the uid has no (or an empty) passwd entry and $HOME is unset;
    # discovery_dir() then uses a per-uid temp dir so uids do not collide.
    try:
        import pwd
        home = pwd.getpwuid(os.getuid()).pw_dir
        if home:
            return home
    except (KeyError, ImportError, OSError):
        pass  # no passwd entry -> try $HOME, then a per-uid temp dir
    return os.environ.get('HOME') or None


def discovery_dir() -> Path:
    override = os.environ.get('OID_AGENT_DIR')
    if override:
        return Path(override)
    if os.name == 'nt':
        local = os.environ.get('LOCALAPPDATA')
        if local:
            return Path(local) / 'oid-agent'
        profile = os.environ.get('USERPROFILE')
        if profile:
            return Path(profile) / 'AppData' / 'Local' / 'oid-agent'
        # No LOCALAPPDATA/USERPROFILE: %TEMP% is already per-user.
        return Path(tempfile.gettempdir()) / 'oid-agent'  # NOSONAR
    home = _home_dir()
    if home:
        return Path(home) / '.oid-agent'
    # No passwd entry and no $HOME: /tmp is shared, so key the fallback by uid
    # to avoid collisions (a dir owned by another uid would be refused).
    return Path(tempfile.gettempdir()) / f'oid-agent-{os.getuid()}'  # NOSONAR


def viewer_discovery_dir() -> Path:
    """Directory holding per-viewer discovery files.

    A ``viewer`` subdirectory of the debugger discovery dir, kept separate
    so the debugger's flat ``*.json`` glob (``live_sessions``) never sees a
    viewer's discovery file, and vice versa.
    """
    return discovery_dir() / 'viewer'


def _is_private_dir(directory: Path) -> bool:
    """
    Whether ``directory`` is a trusted OID discovery directory.

    ``OID_AGENT_DIR`` lets the user redirect discovery anywhere, so a
    misconfiguration (or a hostile symlink) could point it at a shared
    location. A trusted directory meets the same private-dir contract the
    producer enforces (agentendpoint._prepare_discovery_dir): a real
    directory, not a symlink, owned by the current user, with no
    group/other permission bits. Session tokens are only read from -- and
    stale files only deleted in -- a directory that passes this check.
    """
    try:
        if os.path.islink(directory):
            return False
        info = os.stat(directory)
        if not stat.S_ISDIR(info.st_mode):
            return False
        if hasattr(os, 'getuid'):
            if info.st_uid != os.getuid():
                return False
            if stat.S_IMODE(info.st_mode) & 0o077:
                return False
    except OSError:
        return False
    return True


_warned_untrusted: set[str] = set()


def _warn_untrusted_dir(directory: Path) -> None:
    """Warn once (on stderr) that discovery in ``directory`` is refused."""
    key = str(directory)
    if key in _warned_untrusted:
        return
    _warned_untrusted.add(key)
    sys.stderr.write(
        '[oid-mcp] Refusing to read OID discovery files from %r: it is a '
        'symlink, is not owned by you, or is group/world-accessible, so a '
        'session token there cannot be trusted. Point OID_AGENT_DIR at a '
        'private directory you own with mode 0700.\n' % key)


def _pid_alive(pid: int) -> bool:
    if pid <= 0:
        # pid 0 and negatives have special os.kill semantics (current process
        # group / broadcast), so a malformed record with a non-positive pid
        # would look "alive" and never be reaped -- treat it as dead.
        return False
    if os.name == 'nt':
        # os.kill(pid, 0) is NOT a liveness probe on Windows: signal 0 maps to
        # CTRL_C_EVENT and any non-console signal to an unconditional
        # TerminateProcess, so probing would disrupt or kill the very
        # viewer/debugger this enumeration just discovered. Query the process
        # object instead.
        return _pid_alive_windows(pid)
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    except OSError:
        return False
    return True


def _pid_alive_windows(pid: int) -> bool:
    """Non-destructive Windows liveness probe via the process object.

    Opens the process for SYNCHRONIZE and waits with a zero timeout:
    WAIT_TIMEOUT means the object is not signalled (still running), while a
    signalled object means it has exited. This avoids both the destructive
    ``os.kill`` path and the ``GetExitCodeProcess`` STILL_ACTIVE(259)
    ambiguity for a process that exited with code 259.
    """
    import ctypes.wintypes

    ERROR_ACCESS_DENIED = 5
    SYNCHRONIZE = 0x00100000
    WAIT_TIMEOUT = 0x00000102

    wintypes = ctypes.wintypes
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL,
                                     wintypes.DWORD)
    kernel32.WaitForSingleObject.restype = wintypes.DWORD
    kernel32.WaitForSingleObject.argtypes = (wintypes.HANDLE, wintypes.DWORD)
    kernel32.CloseHandle.argtypes = (wintypes.HANDLE,)

    handle = kernel32.OpenProcess(SYNCHRONIZE, False, pid)
    if not handle:
        # No handle: access-denied means the pid is live but owned by another
        # user (mirror POSIX os.kill's PermissionError => alive); any other
        # error (e.g. invalid parameter) means there is no such process.
        return ctypes.get_last_error() == ERROR_ACCESS_DENIED
    try:
        return kernel32.WaitForSingleObject(handle, 0) == WAIT_TIMEOUT
    finally:
        kernel32.CloseHandle(handle)


def _reap(path: Path) -> None:
    """Best-effort removal of a stale/bad discovery file. A stray entry that
    cannot be removed (e.g. a directory left in the dir) must not break
    enumeration, so unlink errors are swallowed."""
    try:
        path.unlink(missing_ok=True)
    except OSError:
        pass  # stray unremovable entry (e.g. a dir); skip, don't crash


def _live_entries(directory: Path, parse):
    """
    Parse every discovery file in a *trusted* directory, reaping the rest.

    Shared by ``live_sessions`` and ``live_viewers``: both hold session
    tokens -- capabilities to a live endpoint that exposes debuggee
    memory -- so both apply the same trust gate before touching anything.
    A symlinked, wrong-owner, or group/world-accessible directory
    (typically a mispointed OID_AGENT_DIR) is untrusted: its tokens may
    have been read or planted by another user, so nothing there is read
    or deleted and we warn once. In a trusted directory, entries that are
    unreadable/unparseable or whose process is gone are unlinked.

    ``parse`` turns a discovery file's ``(path, info_dict)`` into the
    caller's session type; it must expose a ``.pid`` attribute.
    """
    if not directory.is_dir():
        return []
    if not _is_private_dir(directory):
        _warn_untrusted_dir(directory)
        return []
    entries = []
    for path in sorted(directory.glob('*.json')):
        try:
            info = json.loads(path.read_text())
            entry = parse(path, info)
        except (ValueError, KeyError, TypeError, OSError):
            # TypeError: a JSON field is the wrong type (e.g. debugger_pid an
            # object) so int()/float() in parse() rejects it -- treat as a
            # malformed file and reap it rather than aborting enumeration.
            _reap(path)
            continue
        if not _pid_alive(entry.pid):
            _reap(path)
            continue
        entries.append(entry)
    return entries


def _parse_session(path: Path, info: dict) -> SessionInfo:
    return SessionInfo(
        path=path,
        pid=int(info['pid']),
        port=int(info['port']),
        token=str(info['token']),
        debugger=str(info['debugger']),
        start_time=float(info['start_time']),
    )


def _parse_viewer(path: Path, info: dict) -> ViewerSessionInfo:
    debugger_pid = info.get('debugger_pid')
    return ViewerSessionInfo(
        path=path,
        pid=int(info['pid']),
        port=int(info['port']),
        token=str(info['token']),
        start_time=float(info['start_time']),
        debugger_pid=int(debugger_pid) if debugger_pid is not None else None,
    )


def live_sessions() -> list[SessionInfo]:
    """Parse every debugger discovery file in the trusted discovery dir."""
    return _live_entries(discovery_dir(), _parse_session)


def live_viewers() -> list[ViewerSessionInfo]:
    """Parse every viewer discovery file in the trusted viewer subdir."""
    return _live_entries(viewer_discovery_dir(), _parse_viewer)


def pick_session(sessions: list[SessionInfo],
                 session: int | None = None) -> SessionInfo:
    """
    Select a session by debugger pid, or the most recently started one.
    """
    if session is not None:
        for candidate in sessions:
            if candidate.pid == session:
                return candidate
        raise NoSessionError(
            f'no live OID session with pid {session}; '
            f'live pids: {[s.pid for s in sessions]}')
    if not sessions:
        raise NoSessionError(
            'no live OID debug session found. Start the debugger with '
            'OID_AGENT=1 in its environment and source oid.py '
            '(e.g. `OID_AGENT=1 gdb ...` then `source .../oid.py`).')
    return max(sessions, key=lambda s: s.start_time)
