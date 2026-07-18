"""Measure agent round-trip latency against a live (ideally backgrounded) viewer.

Uses one persistent connection so the number isolates the viewer's GL-thread
wake latency (not per-call connect cost). Launch a viewer with OID_AGENT=1,
click another app so it is backgrounded, then run this.

A working condition-variable wake lands ~1 ms regardless of focus. A wake that
depends on the OS event loop (throttled for a non-active window) instead sits
near half a refresh period (~8 ms at 60 Hz) -- imperceptible to "feels
instant", which is why this measures rather than eyeballs.
"""
import argparse
import statistics
import time

from oidmcp import discovery
from oidmcp.protocol import ControlClient


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--calls', type=int, default=100)
    args = parser.parse_args()

    viewers = discovery.live_viewers()
    if not viewers:
        raise SystemExit('no live OID viewer found; launch one with OID_AGENT=1')
    viewer = viewers[0]

    client = ControlClient('127.0.0.1', viewer.port, viewer.token)
    latencies = []
    try:
        for _ in range(args.calls):
            start = time.perf_counter()
            client.get_view()
            latencies.append((time.perf_counter() - start) * 1000)
    finally:
        client.close()

    latencies.sort()
    n = len(latencies)
    print(f'{n} get_view round-trips (persistent connection)')
    print(f'  median={statistics.median(latencies):.3f} ms  '
          f'p90={latencies[int(n * 0.9)]:.3f} ms  '
          f'max={latencies[-1]:.3f} ms')


if __name__ == '__main__':
    main()
