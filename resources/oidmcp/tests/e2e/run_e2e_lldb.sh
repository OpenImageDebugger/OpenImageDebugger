#!/usr/bin/env bash
# End-to-end smoke test: real lldb + real endpoint + real client.
# <deployed-oid-dir> must contain oid.py, oidscripts/ and liboidbridge.
set -euo pipefail

OID_DEPLOY=${1:?usage: run_e2e_lldb.sh <deployed-oid-dir> [oidwindow-bin] [test-image]}
OID_WINDOW_BIN="${OID_WINDOW_BIN:-${2:-}}"
OID_E2E_IMAGE="${OID_E2E_IMAGE:-${3:-}}"
HERE=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$HERE/../../../.." && pwd)
WORK=$(mktemp -d)
cleanup() {
    rm -rf "$WORK"
    if [[ -n "${LLDB_PID:-}" ]]; then
        kill "$LLDB_PID" 2>/dev/null || true
    fi
    if [[ -n "${VIEWER_PID:-}" ]]; then
        kill "$VIEWER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

c++ -g -O0 -I "$REPO_ROOT/src/thirdparty/Eigen" \
    "$HERE/fixture.cpp" -o "$WORK/fixture"

BREAK_LINE=$(grep -n 'BREAK' "$HERE/fixture.cpp" | cut -d: -f1)
cat > "$WORK/cmds.lldb" <<EOF
# Do not let lldb disable ASLR: personality() is blocked by the default
# container seccomp profile even with CAP_SYS_PTRACE, and ASLR is moot here.
settings set target.disable-aslr false
breakpoint set --file fixture.cpp --line $BREAK_LINE
run
command script import $OID_DEPLOY/oid.py
EOF

export OID_AGENT=1
export OID_AGENT_DIR="$WORK/agent"
# RgbFrame matches no builtin entry, so resolving it proves this file was
# read. Exported here: the engine reads it from the debugger's own env.
export OID_TYPES_PATH="$HERE/custom_types.json"

# The bridge runs queue_request() callbacks on its own Python thread, so the
# endpoint answers while lldb sits stopped. `sleep` stdin: lldb quits on EOF.
lldb --no-lldbinit --source "$WORK/cmds.lldb" "$WORK/fixture" < <(sleep 90) &
LLDB_PID=$!

cd "$HERE/../.."

# --frozen: no network re-resolve; --no-build: no dependency build scripts.
# The project itself is imported from source via PYTHONPATH below.
uv sync --frozen --no-build --no-install-project
STATUS=0
OID_AGENT_DIR="$WORK/agent" PYTHONPATH="$PWD" .venv/bin/python tests/e2e/check_session.py \
    || STATUS=$?

kill "$LLDB_PID" 2>/dev/null || true

# Drives a *standalone* viewer window (oidwindow --open <image>, no
# paired debugger) through the same control protocol, exercising
# list_buffers/get_view/set_view/get_buffer on the viewer side of the
# agent endpoint. This needs a real GUI window backed by a display,
# which CI runners don't have, so it only runs when explicitly opted
# into via OID_E2E_VIEWER=1 plus an oidwindow binary and a test image
# (env vars OID_WINDOW_BIN/OID_E2E_IMAGE, or positional args 2/3).
# Treat this as a manual/local check, not a CI gate.
if [[ "${OID_E2E_VIEWER:-0}" != "1" ]]; then
    echo "SKIP: viewer leg (set OID_E2E_VIEWER=1 + OID_WINDOW_BIN + OID_E2E_IMAGE," \
         "or pass them as args 2/3, to run it locally; needs a real display," \
         "so it never runs in CI)"
elif [[ -z "$OID_WINDOW_BIN" || -z "$OID_E2E_IMAGE" ]]; then
    echo "SKIP: OID_E2E_VIEWER=1 but OID_WINDOW_BIN/OID_E2E_IMAGE was not provided"
elif [[ ! -x "$OID_WINDOW_BIN" ]]; then
    echo "SKIP: OID_WINDOW_BIN is not an executable file: $OID_WINDOW_BIN"
elif [[ ! -f "$OID_E2E_IMAGE" ]]; then
    echo "SKIP: OID_E2E_IMAGE not found: $OID_E2E_IMAGE"
else
    # The viewer's endpoint creates $WORK/agent/viewer and its discovery
    # file on startup, so nothing needs preparing beyond the shared dir.
    OID_AGENT=1 OID_AGENT_DIR="$WORK/agent" "$OID_WINDOW_BIN" \
        --open "$OID_E2E_IMAGE" &
    VIEWER_PID=$!

    VIEWER_STATUS=0
    OID_AGENT_DIR="$WORK/agent" PYTHONPATH="$PWD" .venv/bin/python tests/e2e/check_session.py viewer \
        || VIEWER_STATUS=$?

    kill "$VIEWER_PID" 2>/dev/null || true
    if [[ "$VIEWER_STATUS" -ne 0 ]]; then
        STATUS=$VIEWER_STATUS
    fi
fi

exit "$STATUS"
