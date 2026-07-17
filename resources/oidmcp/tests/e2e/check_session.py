"""Poll for a live session and verify the endpoint end-to-end.

Two independent legs, selected by argv[1]:

- ``debugger`` (default): discovers an in-debugger session via
  ``discovery.live_sessions()`` and drives it through hello/list_symbols/
  get_buffer. This is the leg run_e2e_lldb.sh always exercises in CI.
- ``viewer``: discovers a standalone viewer window via
  ``discovery.live_viewers()`` and drives it through hello/list_buffers/
  get_view/set_view/get_buffer. This needs a real GUI window (oidwindow
  backed by a display), so it is not run in CI -- see run_e2e_lldb.sh's
  OID_E2E_VIEWER guard for the manual/local invocation.
"""

import math
import sys
import time

import numpy as np

from oidmcp import discovery
from oidmcp.buffers import decode_buffer
from oidmcp.protocol import ControlClient
from oidmcp.viewer_meta import to_bridge_meta


def check_debugger() -> int:
    deadline = time.time() + 30
    sessions = []
    while time.time() < deadline:
        sessions = discovery.live_sessions()
        if sessions:
            break
        time.sleep(0.5)
    if not sessions:
        print('FAIL: no session appeared within 30s')
        return 1

    session = discovery.pick_session(sessions)
    client = ControlClient('127.0.0.1', session.port, session.token)
    print(f'hello: {client.hello}')

    symbols, _ = client.list_symbols()
    print(f'symbols: {symbols}')
    if 'gradient' not in symbols:
        print('FAIL: gradient not observable')
        return 1

    meta, raw = client.get_buffer('gradient')
    arr = decode_buffer(meta, raw)
    print(f'buffer: shape={arr.shape} dtype={arr.dtype}')
    if arr.shape != (8, 16, 1):
        print('FAIL: unexpected shape')
        return 1
    expected = np.arange(16, dtype=np.float32) / 15.0
    if not np.allclose(arr[0, :, 0], expected):
        print('FAIL: unexpected values')
        return 1

    client.close()
    print('PASS')
    return 0


def check_viewer() -> int:
    deadline = time.time() + 30
    viewers = []
    while time.time() < deadline:
        viewers = discovery.live_viewers()
        if viewers:
            break
        time.sleep(0.5)
    if not viewers:
        print('FAIL: no viewer session appeared within 30s')
        return 1

    # No debugger pairing to resolve against here (a standalone `oidwindow`
    # has no debugger_pid); mirror AgentCore's own "no selector" default and
    # take the most recently started window.
    viewer = max(viewers, key=lambda v: v.start_time)
    client = ControlClient('127.0.0.1', viewer.port, viewer.token)
    print(f'hello: {client.hello}')

    buffers = client.list_viewer_buffers()
    print(f'buffers: {buffers}')
    if not buffers:
        print('FAIL: no buffer open in viewer')
        return 1
    name = buffers[0]['name']

    view = client.get_view()
    print(f'view: {view}')

    client.set_view(zoom=2.0, rotation_deg=90, channel=1)
    readback = client.get_view()
    print(f'view after set_view: {readback}')
    # zoom/rotation_deg round-trip through float math (e.g. a zoom-power
    # cast), so compare with a tolerance. channel comes back as the
    # "0".."2"/"all" string the model tracks (see ViewState::channel), not
    # the integer index the request sent.
    if (not math.isclose(readback['zoom'], 2.0, rel_tol=1e-3)
            or not math.isclose(readback['rotation_deg'], 90.0, abs_tol=1e-3)
            or readback['channel'] != '1'):
        print('FAIL: set_view did not stick (readback mismatch)')
        return 1

    meta, raw = client.get_buffer(name)
    arr = decode_buffer(to_bridge_meta(meta), raw)
    print(f'buffer: shape={arr.shape} dtype={arr.dtype}')

    client.close()
    print('PASS')
    return 0


def main() -> int:
    mode = sys.argv[1] if len(sys.argv) > 1 else 'debugger'
    if mode == 'viewer':
        return check_viewer()
    return check_debugger()


if __name__ == '__main__':
    sys.exit(main())
