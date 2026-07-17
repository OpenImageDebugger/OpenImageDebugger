#!/usr/bin/env bash
# End-to-end smoke test: real lldb + real endpoint + real client.
# Usage: run_e2e_lldb.sh <deployed-oid-dir> [oidwindow-bin] [test-image]
#   <deployed-oid-dir> must contain oid.py, oidscripts/, liboidbridge.
#   [oidwindow-bin]/[test-image], or the OID_WINDOW_BIN/OID_E2E_IMAGE env
#   vars, feed the optional viewer leg below (see OID_E2E_VIEWER).
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
# Do not let lldb disable ASLR: that uses the personality() syscall, which
# is blocked by the default container seccomp profile even with
# CAP_SYS_PTRACE. ASLR is irrelevant to this smoke test.
settings set target.disable-aslr false
breakpoint set --file fixture.cpp --line $BREAK_LINE
run
command script import $OID_DEPLOY/oid.py
EOF

export OID_AGENT=1
export OID_AGENT_DIR="$WORK/agent"

# `command script import oid.py` runs its main(), which builds the
# LldbBridge; that bridge runs queue_request() callbacks on its own
# Python thread, so the endpoint answers while lldb sits stopped at the
# breakpoint. stdin from `sleep` only keeps the session alive for the
# checker (lldb quits on EOF).
lldb --no-lldbinit --source "$WORK/cmds.lldb" "$WORK/fixture" < <(sleep 90) &
LLDB_PID=$!

cd "$HERE/../.."
STATUS=0
OID_AGENT_DIR="$WORK/agent" uv run python tests/e2e/check_session.py \
    || STATUS=$?

kill "$LLDB_PID" 2>/dev/null || true

# --- Optional viewer-endpoint leg -----------------------------------
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
    # The viewer's own agent endpoint creates $WORK/agent/viewer (and its
    # discovery file) on startup; nothing to prepare here beyond the dir
    # already shared with the debugger leg above.
    OID_AGENT=1 OID_AGENT_DIR="$WORK/agent" "$OID_WINDOW_BIN" \
        --open "$OID_E2E_IMAGE" &
    VIEWER_PID=$!

    VIEWER_STATUS=0
    OID_AGENT_DIR="$WORK/agent" uv run python tests/e2e/check_session.py viewer \
        || VIEWER_STATUS=$?

    kill "$VIEWER_PID" 2>/dev/null || true
    if [[ "$VIEWER_STATUS" -ne 0 ]]; then
        STATUS=$VIEWER_STATUS
    fi
fi

exit "$STATUS"
