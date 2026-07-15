"""MCP server exposing OpenImageDebugger buffer inspection to AI agents."""

import sys as _sys
from pathlib import Path as _Path

# oid-mcp runs from the OID source tree (`uv run --directory
# resources/oidmcp`). Put `resources/` on sys.path so the stdlib-only
# framing shared with the in-debugger endpoint (`oidscripts.wireframe`)
# imports at runtime, exactly as the test conftest does for pytest.
# Append (lowest precedence) so this never shadows a similarly named
# top-level module elsewhere on the path.
_RESOURCES_DIR = str(_Path(__file__).resolve().parents[2])
if _RESOURCES_DIR not in _sys.path:
    _sys.path.append(_RESOURCES_DIR)

__version__ = "0.1.0"
