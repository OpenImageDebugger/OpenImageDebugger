#!/usr/bin/env bash
# Assemble wasm/dist from a successful oidwindow WASM build.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_SRC="${BUILD_SRC:-$ROOT/build-wasm/src}"
DIST="$ROOT/wasm/dist"

die() {
  echo "error: $*" >&2
  exit 1
}

[[ -f "$BUILD_SRC/oidwindow.wasm" ]] || die "missing $BUILD_SRC/oidwindow.wasm (run wasm/scripts/build-wasm.sh first)"

rm -rf "$DIST"
mkdir -p "$DIST"

cp "$BUILD_SRC/oidwindow.wasm" "$BUILD_SRC/oidwindow.js" "$DIST/"
cp "$ROOT/wasm/loader.html" "$DIST/"
cp "$ROOT/wasm/demo.html" "$DIST/index.html"
cp "$ROOT/wasm/demo-buffers.js" "$DIST/"
cp "$ROOT/wasm/test-harness.html" "$DIST/"
cp "$BUILD_SRC/qtloader.js" "$DIST/"

# Optional Qt assets emitted beside the build output.
cp "$BUILD_SRC"/qt*.js "$BUILD_SRC"/qt*.wasm "$DIST/" 2>/dev/null || true
cp "$BUILD_SRC/qtlogo.svg" "$DIST/" 2>/dev/null || true

OID_REV="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo dev)"
echo "{\"qt\":\"6.11\",\"emscripten\":\"4.0.7\",\"oid\":\"$OID_REV\"}" > "$DIST/version.json"

echo "Packed viewer WASM artifact to $DIST"
ls -lh "$DIST"
