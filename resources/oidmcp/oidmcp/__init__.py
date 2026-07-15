"""MCP server exposing OpenImageDebugger buffer inspection to AI agents.

Importing this package has no side effects beyond its own boundary. The
stdlib-only wire framing shared with the in-debugger endpoint is
vendored as ``oidmcp._wireframe`` (a byte-identical copy of
``oidscripts/wireframe.py``, kept in sync by a test), so nothing here
mutates the interpreter-wide ``sys.path`` or loads code by filesystem
path.
"""

__version__ = "0.1.0"
