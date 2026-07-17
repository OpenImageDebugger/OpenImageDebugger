"""MCP server exposing OpenImageDebugger buffer inspection to AI agents.

The stdlib-only wire framing is defined once, in
``oidscripts.wireframe``, and shared with the in-debugger endpoint. The
``oidmcp._wireframe`` bridge re-exports it, appending the sibling
debugger-scripts tree to ``sys.path`` (idempotent and lowest-precedence)
so it imports as a normal named package rather than being loaded by
filesystem path.
"""

__version__ = "0.2.0"
