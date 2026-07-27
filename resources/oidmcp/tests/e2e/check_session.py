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

    status = check_custom_type(client, symbols)
    client.close()
    if status:
        return status
    print('PASS')
    return 0


def check_custom_type(client, symbols) -> int:
    """
    Resolve the fixture's RgbFrame, which no builtin entry matches.

    Reaching it at all proves the engine loaded custom_types.json from
    OID_TYPES_PATH; reading the right pixels out of it proves the JSON's
    field expressions were evaluated against the real debugger rather than
    guessed. row_stride carries the weight here: the fixture pads every row
    to 60 bytes (20 pixels) against a 16-pixel width, so a stride the engine
    computed as the width would still return a correct row 0 and garbage
    afterwards.
    """
    if 'frame' not in symbols:
        print('FAIL: frame not observable -- custom_types.json did not load')
        return 1

    meta, raw = client.get_buffer('frame')
    print(f'custom type meta: {meta}')
    if int(meta['row_stride']) != 20:
        print(f"FAIL: row_stride {meta['row_stride']}, expected 20 pixels "
              '(60 bytes / 3 channels / 1 byte)')
        return 1

    arr = decode_buffer(meta, raw)
    print(f'custom buffer: shape={arr.shape} dtype={arr.dtype}')
    if arr.shape != (8, 16, 3) or arr.dtype != np.uint8:
        print('FAIL: unexpected shape or dtype for the custom type')
        return 1

    rows, cols = np.mgrid[0:8, 0:16]
    expected = np.stack([(cols * 16) & 0xff,
                         (rows * 32) & 0xff,
                         ((rows + cols) * 8) & 0xff],
                        axis=-1).astype(np.uint8)
    if not np.array_equal(arr, expected):
        # The last row is the one a wrong stride corrupts, so name it.
        print('FAIL: custom buffer pixels differ; '
              f'row 0 ok={np.array_equal(arr[0], expected[0])} '
              f'row 7 ok={np.array_equal(arr[7], expected[7])}')
        return 1

    # Padding must never reach the caller: 0xff anywhere means whole rows
    # were taken from the gaps rather than de-strided past them.
    if (arr == 0xff).all(axis=2).any():
        print('FAIL: saturated pixel found -- padding leaked into the buffer')
        return 1

    print('custom type: PASS')
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
