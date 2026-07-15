"""MCP server exposing OpenImageDebugger buffer inspection to AI agents.

Importing this package has no side effects beyond its own boundary. The
one module that needs the stdlib-only framing shared with the in-debugger
endpoint (`protocol`) loads it directly from its file, so nothing here
mutates the interpreter-wide ``sys.path``.
"""

__version__ = "0.1.0"
