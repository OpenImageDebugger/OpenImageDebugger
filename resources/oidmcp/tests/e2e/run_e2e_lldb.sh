#!/usr/bin/env bash
# End-to-end smoke test: real lldb + real endpoint + real client.
# Usage: run_e2e_lldb.sh <deployed-oid-dir>
#   <deployed-oid-dir> must contain oid.py, oidscripts/, liboidbridge.
set -euo pipefail

OID_DEPLOY=${1:?usage: run_e2e_lldb.sh <deployed-oid-dir>}
HERE=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$HERE/../../../.." && pwd)
WORK=$(mktemp -d)
cleanup() {
    rm -rf "$WORK"
    if [[ -n "${LLDB_PID:-}" ]]; then
        kill "$LLDB_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

c++ -g -O0 -I "$REPO_ROOT/src/thirdparty/Eigen" \
    "$HERE/fixture.cpp" -o "$WORK/fixture"

BREAK_LINE=$(grep -n 'BREAK' "$HERE/fixture.cpp" | cut -d: -f1)
cat > "$WORK/cmds.lldb" <<EOF
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
lldb --source "$WORK/cmds.lldb" "$WORK/fixture" < <(sleep 90) &
LLDB_PID=$!

cd "$HERE/../.."
STATUS=0
OID_AGENT_DIR="$WORK/agent" uv run python tests/e2e/check_session.py \
    || STATUS=$?

kill "$LLDB_PID" 2>/dev/null || true
exit "$STATUS"
