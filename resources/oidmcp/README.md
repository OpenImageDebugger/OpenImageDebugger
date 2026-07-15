# oid-mcp

MCP server that gives AI agents eyes on OpenImageDebugger buffers in a
live gdb/lldb session: list observable symbols at a breakpoint, view
renderings, read exact values, dump lossless `.npy` copies, and mirror
buffers into the human viewer.

## Setup

1. Start your debugger with the agent endpoint enabled and OID loaded:

   ```bash
   OID_AGENT=1 gdb ./your-program
   (gdb) source /path/to/OpenImageDebugger/oid.py
   ```

   (lldb works the same; `command script import .../oid.py`.)

2. Register the server with your MCP client. For Claude Code,
   `.mcp.json`:

   ```json
   {
     "mcpServers": {
       "oid": {
         "command": "uv",
         "args": ["run", "--directory",
                  "/path/to/OpenImageDebugger/resources/oidmcp",
                  "oid-mcp"]
       }
     }
   }
   ```

## Tools

| Tool | Purpose |
| --- | --- |
| `list_sessions()` | Live debug sessions (all tools take `session=<pid>`) |
| `list_buffers()` | Observable buffer symbols at the current stop |
| `view(symbol, region?, channel?, vmin?, vmax?, max_px?)` | PNG rendering; `region` is the stateless zoom |
| `stats(symbol, region?)` | Per-channel min/max/mean/std, NaN/Inf/zero counts |
| `values(symbol, x, y, w, h, channel?)` | Exact numbers (≤1024 per call) |
| `dump(symbol, path?)` | Lossless `.npy` dump |
| `plot(symbol)` | Mirror into the human viewer window |

## Notes

- The endpoint only starts when `OID_AGENT=1` is set; it binds
  localhost with a per-session token (discovery files under
  `$TMPDIR/oid-agent-<user>/`, override with `OID_AGENT_DIR`). It
  exposes debuggee memory to local processes that can read those
  files — the same trust domain as your user account.
- Buffers transfer fully on first access per stop and are cached;
  the cap is `OID_MCP_MAX_BYTES` (default 256 MiB).
- The debuggee must be stopped for symbol evaluation; while it runs,
  tools return a retryable error.
- The viewer window still opens on the first debugger stop (normal
  OID behavior); `plot` requires it, the other tools do not.
