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
does to import from the debugger-scripts tree.
"""

import sys
from pathlib import Path

# resources/oidmcp/oidmcp/_wireframe.py -> parents[2] == resources/
_RESOURCES_DIR = str(Path(__file__).resolve().parents[2])
if _RESOURCES_DIR not in sys.path:
    sys.path.append(_RESOURCES_DIR)

try:
    from oidscripts.wireframe import (  # noqa: E402
        MAX_FRAME_BYTES,
        recv_frame,
        send_frame,
    )
except ImportError as exc:  # pragma: no cover - deployment misconfiguration
    raise ImportError(
        'oid-mcp requires the OpenImageDebugger scripts tree; expected '
        'oidscripts alongside this package under resources/. Run oid-mcp '
        'from the OID install (uv run --directory resources/oidmcp).'
    ) from exc

__all__ = ['MAX_FRAME_BYTES', 'recv_frame', 'send_frame']
