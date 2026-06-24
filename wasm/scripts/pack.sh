#!/usr/bin/env bash
# Pack viewer WASM into wasm/dist and optionally sync to the VS Code extension.
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: wasm/scripts/pack.sh [options]

Assemble wasm/dist from a successful oidwindow WASM build. By default also
copies artifacts into the VS Code extension media/ folder.

Environment overrides:
  BUILD_SRC         Build output directory (default: <repo>/build-wasm/src)
  EXTENSION_MEDIA   Extension media directory
                    (default: <repo>/../openimagedebugger-vscode/media)

Options:
  -h, --help          Show this help
  --build             Run wasm/scripts/build-wasm.sh before packing
  --no-extension      Skip copying to EXTENSION_MEDIA
  --extension-dir DIR Override extension media destination

Examples:
  wasm/scripts/pack.sh
  wasm/scripts/pack.sh --build
  wasm/scripts/pack.sh --no-extension
  EXTENSION_MEDIA=~/my-ext/media wasm/scripts/pack.sh
EOF
}

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_SRC="${BUILD_SRC:-$ROOT/build-wasm/src}"
EXTENSION_MEDIA="${EXTENSION_MEDIA:-$ROOT/../openimagedebugger-vscode/media}"

BUILD=0
SYNC_EXTENSION=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --build)
      BUILD=1
      shift
      ;;
    --no-extension)
      SYNC_EXTENSION=0
      shift
      ;;
    --extension-dir)
      [[ $# -ge 2 ]] || {
        echo "error: --extension-dir requires a path" >&2
        exit 1
      }
      EXTENSION_MEDIA="$2"
      shift 2
      ;;
    *)
      echo "error: unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ "$BUILD" -eq 1 ]]; then
  "$SCRIPT_DIR/build-wasm.sh"
fi

export BUILD_SRC
"$SCRIPT_DIR/pack-viewer-wasm.sh"

if [[ "$SYNC_EXTENSION" -eq 1 ]]; then
  mkdir -p "$EXTENSION_MEDIA"
  rm -rf "${EXTENSION_MEDIA:?}/"*
  cp -r "$ROOT/wasm/dist/"* "$EXTENSION_MEDIA/"
  echo
  echo "Synced viewer WASM to $EXTENSION_MEDIA"
  ls -lh "$EXTENSION_MEDIA"
fi
