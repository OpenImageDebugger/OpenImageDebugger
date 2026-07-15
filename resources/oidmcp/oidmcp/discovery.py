"""Locate live OID debug sessions via their discovery files."""

from __future__ import annotations

import getpass
import json
import os
import stat
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


def discovery_dir() -> Path:
    override = os.environ.get('OID_AGENT_DIR')
    if override:
        return Path(override)
    try:
        user = getpass.getuser()
    except Exception:
        user = str(os.getuid()) if hasattr(os, 'getuid') else 'user'
    return Path(tempfile.gettempdir()) / f'oid-agent-{user}'


def _cleanup_allowed(directory: Path) -> bool:
    """
    Whether it is safe to unlink stale/unreadable files under ``directory``.

    ``OID_AGENT_DIR`` lets the user redirect discovery anywhere, so a
    misconfiguration (or a hostile symlink) could otherwise make this
    module delete unrelated ``*.json`` files. Only a directory that meets
    the same private-dir contract the producer enforces
    (agentendpoint._prepare_discovery_dir) may be cleaned: it must be a
    real directory, not a symlink, owned by the current user, with no
    group/other permission bits. When the directory fails this check we
    still list whatever sessions parse, but delete nothing.
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


def _pid_alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    except OSError:
        return False
    return True


def live_sessions() -> list[SessionInfo]:
    """
    Parse every discovery file. Entries that are unreadable or whose
    debugger process is gone are unlinked, but only when the directory
    passes _cleanup_allowed(); in an untrusted/misconfigured
    OID_AGENT_DIR nothing is deleted.
    """
    directory = discovery_dir()
    if not directory.is_dir():
        return []
    may_delete = _cleanup_allowed(directory)
    sessions = []
    for path in sorted(directory.glob('*.json')):
        try:
            info = json.loads(path.read_text())
            session = SessionInfo(
                path=path,
                pid=int(info['pid']),
                port=int(info['port']),
                token=str(info['token']),
                debugger=str(info['debugger']),
                start_time=float(info['start_time']),
            )
        except (ValueError, KeyError, OSError):
            if may_delete:
                path.unlink(missing_ok=True)
            continue
        if not _pid_alive(session.pid):
            if may_delete:
                path.unlink(missing_ok=True)
            continue
        sessions.append(session)
    return sessions


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
