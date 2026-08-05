# Changelog

All notable changes to `oid-mcp` are documented here. This project adheres to
[Semantic Versioning](https://semver.org/).

## [0.3.4] — 2026-08-05

### Added
- The README is now the package's PyPI description. The project page was blank
  before this, since `pyproject.toml` never declared a `readme`.

### Fixed
- Corrected the MCP registry ownership marker in the README, which still
  carried the lowercased namespace. Together with the description above this
  lets the registry verify the package and list the server, which it refused to
  do for 0.3.2 and 0.3.3.

## [0.3.3] — 2026-08-05

### Fixed
- Corrected the MCP registry namespace in `server.json` to
  `io.github.OpenImageDebugger/oid-mcp`. The lowercased spelling did not match
  the GitHub organisation, so the registry rejected the 0.3.2 listing and the
  server never appeared there. The published package is otherwise identical to
  0.3.2.

## [0.3.2] — 2026-08-05

### Changed
- Refreshed the `oidscripts` copy bundled in the wheel (declarative custom
  buffer types, JSON resolver entry points). The server's own code is
  unchanged, as is `oidscripts.wireframe`, the one bundled module it imports
  at runtime, so behaviour against a live session is the same as 0.3.1.

### Packaging
- First release carrying `server.json`, so this version also lists the server
  on the MCP registry.
- Refreshed the dev lockfile (`cryptography`). The published runtime
  dependencies (`mcp`, `numpy`, `pillow`) are unchanged.

## [0.2.0] — 2026-07-17

### Added
- **Viewer sessions.** Kind-aware discovery of live `oidwindow` viewer windows
  (a separate `viewer/` discovery subdir), so an agent can inspect a standalone
  `oidwindow --open` session with no debugger attached.
- **`set_view` / `get_view` tools.** Drive a viewer window absolutely — buffer,
  center, zoom, `rotation_deg`, `channel` (`0`/`1`/`2`/`"all"`), and the global
  `auto_contrast` toggle — and read its current state back.
- **Viewer-session routing.** `list_sessions()` now reports both debuggers and
  viewers plus the viewer↔debugger pairing (`debugger_pid`); the pixel tools
  (`list_buffers`/`view`/`stats`/`values`/`dump`) fall back to a viewer window
  when no live debugger session matches the selector.

### Changed
- `set_view`'s `channel` accepts an integer index (`0`/`1`/`2`) as well as
  `"all"`, matching the documented indices and the other tools.

## [0.1.0]

### Added
- Initial release: debugger-session discovery and the `list_sessions`,
  `list_buffers`, `view`, `stats`, `values`, and `dump` tools over a live
  gdb/lldb session.
