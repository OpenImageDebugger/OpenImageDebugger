#!/usr/bin/env bash
# Serve wasm/dist on a single port (kills stale test servers first).
set -euo pipefail

PORT="${PORT:-8765}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DIST="$ROOT/wasm/dist"

[[ -f "$DIST/loader.html" ]] || {
  echo "error: $DIST/loader.html missing — run wasm/scripts/build-wasm.sh && wasm/scripts/pack-viewer-wasm.sh" >&2
  exit 1
}

if pids=$(lsof -ti ":${PORT}" 2>/dev/null); then
  echo "Stopping existing server on port ${PORT} (pid: ${pids//$'\n'/ })"
  kill ${pids} 2>/dev/null || true
  sleep 0.5
fi

echo "Serving $DIST at http://localhost:${PORT}/test-harness.html"
echo "Close all other OID WASM browser tabs before testing."
cd "$DIST"
exec python3 -m http.server "$PORT"
