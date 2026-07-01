#!/usr/bin/env bash
# Build oidwindow for Qt WebAssembly (Emscripten).
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: wasm/scripts/build-wasm.sh [options]

Configure and build the OID WASM viewer (oidwindow).

Environment overrides:
  EMSDK          Path to emsdk checkout (default: ~/emsdk)
  QT_WASM_DIR    Qt WASM kit root (default: ~/Qt/6.11.0/wasm_singlethread)
  QT_HOST_PATH   Qt macOS/desktop host kit (default: ~/Qt/6.11.0/macos)
  BUILD_DIR      CMake build directory (default: <repo>/build-wasm)
  BUILD_TYPE     CMake build type (default: Release)
  JOBS           Parallel build jobs (default: detected CPU count)

Options:
  -h, --help     Show this help
  --clean        Remove BUILD_DIR before configuring
  --configure    Configure only (no compile)
  --reconfigure  Force CMake reconfigure

Outputs (under BUILD_DIR/src/):
  oidwindow.wasm  oidwindow.js  oidwindow.html  qtloader.js
EOF
}

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

EMSDK="${EMSDK:-$HOME/emsdk}"
QT_WASM_DIR="${QT_WASM_DIR:-$HOME/Qt/6.11.0/wasm_singlethread}"
QT_HOST_PATH="${QT_HOST_PATH:-$HOME/Qt/6.11.0/macos}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-wasm}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)}"

CLEAN=0
CONFIGURE_ONLY=0
RECONFIGURE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --clean)
      CLEAN=1
      shift
      ;;
    --configure)
      CONFIGURE_ONLY=1
      shift
      ;;
    --reconfigure)
      RECONFIGURE=1
      shift
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

QT_CMAKE="$QT_WASM_DIR/bin/qt-cmake"
EMSDK_ENV="$EMSDK/emsdk_env.sh"

die() {
  echo "error: $*" >&2
  exit 1
}

[[ -f "$EMSDK_ENV" ]] || die "emsdk not found at $EMSDK (set EMSDK)"
[[ -x "$QT_CMAKE" ]] || die "qt-cmake not found at $QT_CMAKE (set QT_WASM_DIR)"
[[ -d "$QT_HOST_PATH" ]] || die "Qt host kit not found at $QT_HOST_PATH (set QT_HOST_PATH)"

# shellcheck source=/dev/null
source "$EMSDK_ENV"
export EMSDK

if [[ "$CLEAN" -eq 1 ]]; then
  echo "Removing $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

CMAKE_ARGS=(
  -S "$ROOT"
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
  -DQT_HOST_PATH="$QT_HOST_PATH"
  -DBUILD_TESTS=OFF
)

if [[ "$RECONFIGURE" -eq 1 ]] || [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  echo "Configuring WASM build in $BUILD_DIR"
  "$QT_CMAKE" "${CMAKE_ARGS[@]}"
else
  echo "Using existing CMake cache in $BUILD_DIR (pass --reconfigure to refresh)"
fi

if [[ "$CONFIGURE_ONLY" -eq 1 ]]; then
  exit 0
fi

echo "Building oidwindow ($JOBS jobs)"
cmake --build "$BUILD_DIR" --target oidwindow -j"$JOBS"

OUT_DIR="$BUILD_DIR/src"
echo
echo "WASM build complete:"
ls -lh "$OUT_DIR"/oidwindow.{wasm,js,html} "$OUT_DIR"/qtloader.js 2>/dev/null || true
echo
echo "Serve locally:"
echo "  cd $OUT_DIR && python3 -m http.server 8765"
echo "  open http://localhost:8765/oidwindow.html"
