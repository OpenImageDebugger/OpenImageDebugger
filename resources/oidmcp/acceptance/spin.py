"""Animate the view (rotation + zoom + pan) to confirm a backgrounded viewer renders.

Launch a viewer with OID_AGENT=1 and a buffer (see gen_image.py), background it,
then run this and watch: smooth rotation, zoom pulsing, and the center orbiting
means the paced render loop keeps running at the monitor rate while the window
is inactive. The printed latency is the per-update round-trip.

All three motions are driven through set_view, so they work on any loaded buffer
-- including one pushed over the debugger IPC path.
"""
import argparse
import math
import statistics
import time

from oidmcp import discovery
from oidmcp.protocol import ControlClient


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=12.0)
    parser.add_argument('--fps', type=float, default=60.0)
    parser.add_argument('--pan-radius', type=float, default=50.0,
                        help='center orbit radius in buffer pixels')
    args = parser.parse_args()

    viewers = discovery.live_viewers()
    if not viewers:
        raise SystemExit(
            'no live OID viewer found; launch one with OID_AGENT=1 and a buffer')
    viewer = viewers[0]

    client = ControlClient('127.0.0.1', viewer.port, viewer.token)
    origin = client.get_view().get('center', [0.0, 0.0])  # orbit around current center
    print(f'Animating the (ideally backgrounded) window for ~{args.seconds:.0f}s '
          '-- rotation + zoom + pan; watch it move smoothly...')

    latencies = []
    period = 1.0 / args.fps
    start = time.perf_counter()
    end = start + args.seconds
    next_tick = start
    try:
        while time.perf_counter() < end:
            t = time.perf_counter() - start
            rotation = (t * 90.0) % 360.0                    # 90 deg/s
            zoom = 2.0 + 1.2 * math.sin(t * 1.5)             # pulse ~0.8..3.2
            orbit = t * 2.0                                  # rad/s
            center = [origin[0] + args.pan_radius * math.cos(orbit),
                      origin[1] + args.pan_radius * math.sin(orbit)]
            call = time.perf_counter()
            client.set_view(rotation_deg=rotation, zoom=zoom, center=center)
            latencies.append((time.perf_counter() - call) * 1000)
            next_tick += period
            slack = next_tick - time.perf_counter()
            if slack > 0:
                time.sleep(slack)
        client.set_view(rotation_deg=0.0, zoom=3.5, center=origin)  # clean view
    finally:
        client.close()

    latencies.sort()
    n = len(latencies)
    print(f'sent {n} set_view updates (~{n / args.seconds:.0f}/s)')
    print(f'  latency median={statistics.median(latencies):.2f} ms  '
          f'p90={latencies[int(n * 0.9)]:.2f} ms  '
          f'max={latencies[-1]:.2f} ms')


if __name__ == '__main__':
    main()
