"""MCP server exposing OpenImageDebugger buffer inspection to AI agents."""

import sys as _sys
from pathlib import Path as _Path

# oid-mcp runs from the OID source tree (`uv run --directory
# resources/oidmcp`). Put `resources/` on sys.path so the stdlib-only
# framing shared with the in-debugger endpoint (`oidscripts.wireframe`)
# imports at runtime, exactly as the test conftest does for pytest.
_RESOURCES_DIR = _Path(__file__).resolve().parents[2]
if str(_RESOURCES_DIR) not in _sys.path:
    _sys.path.insert(0, str(_RESOURCES_DIR))

__version__ = "0.1.0"
