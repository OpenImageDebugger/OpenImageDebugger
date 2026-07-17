# Changelog

All notable changes to `oid-mcp` are documented here. This project adheres to
[Semantic Versioning](https://semver.org/).

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
