"""Locate live OID debug sessions via their discovery files."""

from __future__ import annotations

import getpass
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


def discovery_dir() -> Path:
    override = os.environ.get('OID_AGENT_DIR')
    if override:
        return Path(override)
    try:
        user = getpass.getuser()
    except Exception:
        user = str(os.getuid()) if hasattr(os, 'getuid') else 'user'
    return Path(tempfile.gettempdir()) / f'oid-agent-{user}'


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
    Parse every discovery file in a *trusted* discovery directory.

    The directory holds session tokens -- capabilities to a live debug
    endpoint that exposes debuggee memory. A symlinked, wrong-owner, or
    group/world-accessible directory (typically a mispointed
    OID_AGENT_DIR) is untrusted: its tokens may have been read or planted
    by another user, so nothing there is read or deleted and we warn once.
    In a trusted directory, entries that are unreadable or whose debugger
    process is gone are unlinked.
    """
    directory = discovery_dir()
    if not directory.is_dir():
        return []
    if not _is_private_dir(directory):
        _warn_untrusted_dir(directory)
        return []
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
            path.unlink(missing_ok=True)
            continue
        if not _pid_alive(session.pid):
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
