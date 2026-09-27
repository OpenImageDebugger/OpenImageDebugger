# -*- coding: utf-8 -*-

"""Bridge to the single source of the wire framing: ``oidscripts.wireframe``.

oid-mcp and the in-debugger endpoint must agree byte-for-byte on the wire
format. Rather than keep a second copy in step by hand, both sides import
the one module that defines it. oid-mcp runs from the deployed OID
resources tree (``uv run --directory resources/oidmcp``), where
``resources/oidscripts`` is a sibling of this package, so this bridge puts
that directory on ``sys.path`` and re-exports the shared helpers as a
normal named-package import -- no code is loaded by filesystem path.

The append is idempotent and lowest-precedence, so it cannot shadow a
stdlib or site module; it mirrors what the test suite's conftest already
does to import from the debugger-scripts tree. That low precedence,
however, also means an unrelated ``oidscripts`` earlier on ``sys.path``
could win the import, and a foreign ``wireframe`` may carry an
incompatible wire format. So the resolved module is verified to be the one
shipped beside us and a mismatch is rejected loudly rather than used
silently.
"""

import os
import sys
import sysconfig
from pathlib import Path

# Three layouts hold the shared tree: repo (parents[2]), wheel-installed
# (parents[1]), and editable, where only purelib gets the force-include.
_RESOURCES_DIR = Path(__file__).resolve().parents[2]
_SITE_DIR = Path(__file__).resolve().parents[1]
_PURELIB_DIR = Path(sysconfig.get_path('purelib'))
_OIDSCRIPTS_WIREFRAME = Path('oidscripts') / 'wireframe.py'
_EXPECTED_WIREFRAMES = (
    _RESOURCES_DIR / _OIDSCRIPTS_WIREFRAME,
    _SITE_DIR / _OIDSCRIPTS_WIREFRAME,
    _PURELIB_DIR / _OIDSCRIPTS_WIREFRAME,
)
if str(_RESOURCES_DIR) not in sys.path:
    sys.path.append(str(_RESOURCES_DIR))

try:
    from oidscripts import wireframe as _wireframe  # noqa: E402
except ImportError as exc:  # pragma: no cover - deployment misconfiguration
    raise ImportError(
        'oid-mcp requires the OpenImageDebugger scripts tree; expected '
        'oidscripts alongside this package under resources/. Run oid-mcp '
        'from the OID install (uv run --directory resources/oidmcp).'
    ) from exc

# An earlier sys.path entry could resolve `oidscripts` to another tree
# whose wireframe encodes frames incompatibly; reject it, never use it.
_loaded = getattr(_wireframe, '__file__', None)
if _loaded is not None and os.path.realpath(_loaded) not in {
        os.path.realpath(str(p)) for p in _EXPECTED_WIREFRAMES}:
    raise ImportError(  # pragma: no cover - conflicting oidscripts on path
        'oid-mcp imported a foreign oidscripts.wireframe %s; expected '
        'one of %s. Remove the conflicting oidscripts from sys.path / '
        'PYTHONPATH.'
        % (_loaded, [str(p) for p in _EXPECTED_WIREFRAMES]))

MAX_FRAME_BYTES = _wireframe.MAX_FRAME_BYTES
recv_frame = _wireframe.recv_frame
send_frame = _wireframe.send_frame

__all__ = ['MAX_FRAME_BYTES', 'recv_frame', 'send_frame']
