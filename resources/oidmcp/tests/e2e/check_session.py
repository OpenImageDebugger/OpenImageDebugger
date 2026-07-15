"""Poll for the lldb session and verify the endpoint end-to-end."""

import sys
import time

import numpy as np

from oidmcp import discovery
from oidmcp.buffers import decode_buffer
from oidmcp.protocol import ControlClient


def main() -> int:
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


if __name__ == '__main__':
    sys.exit(main())
