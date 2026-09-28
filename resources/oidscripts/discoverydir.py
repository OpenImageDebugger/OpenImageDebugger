# -*- coding: utf-8 -*-

"""
Shared, stdlib-only resolution of the agent discovery directory.

Single source of the directory: imported both by the in-debugger endpoint
(oidscripts.agentendpoint), which writes the discovery files, and by the
oid-mcp client (oidmcp.discovery), which reads them. The two must resolve
the identical directory or neither ever sees the other. Stdlib-only so it
loads inside whatever Python the debugger embeds.
"""

import os
import tempfile


def _home_dir():
    """Passwd-database home so a debugger endpoint and a stripped-env client
    resolve the same dir independent of $HOME/$TMPDIR/$XDG_*. Returns None when
    the uid has no (or an empty) passwd entry and $HOME is unset; discovery_dir()
    then uses a per-uid temp dir so uids do not collide. Mirrors the native
    home_dir()."""
    try:
        import pwd
        home = pwd.getpwuid(os.getuid()).pw_dir
        if home:
            return home
    except (KeyError, ImportError, OSError):
        pass  # no passwd entry -> try $HOME, then a per-uid temp dir
    return os.environ.get('HOME') or None


def discovery_dir():
    """
    Directory holding per-session discovery files. Overridable with
    OID_AGENT_DIR (an absolute path; ~ is not expanded); defaults to the
    passwd-database home / .oid-agent (%LOCALAPPDATA%\\oid-agent on Windows),
    stable across launch contexts.
    """
    override = os.environ.get('OID_AGENT_DIR')
    if override:
        return override
    if os.name == 'nt':
        local = os.environ.get('LOCALAPPDATA')
        if local:
            return os.path.join(local, 'oid-agent')
        profile = os.environ.get('USERPROFILE')
        if profile:
            return os.path.join(profile, 'AppData', 'Local', 'oid-agent')
        # No LOCALAPPDATA/USERPROFILE: %TEMP% is already per-user.
        return os.path.join(tempfile.gettempdir(), 'oid-agent')  # NOSONAR
    home = _home_dir()
    if home:
        return os.path.join(home, '.oid-agent')
    # No passwd entry and no $HOME: /tmp is shared, so key the fallback by uid.
    return os.path.join(tempfile.gettempdir(),
                        'oid-agent-%d' % os.getuid())  # NOSONAR
