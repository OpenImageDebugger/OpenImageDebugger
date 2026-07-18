# Agent-endpoint acceptance scripts

Live-acceptance harness for the OID viewer's agent endpoint. These verify the
property that unit tests cannot: that a **backgrounded** viewer window still
services agent requests promptly. That behaviour depends on runtime OS event
scheduling (macOS throttles a non-active window's event loop), so it must be
measured against a real, backgrounded window.

They talk to a live viewer through the same `oidmcp` client the MCP server
uses, so they must run in the package's environment (`uv run`).

## Why "backgrounded" matters

The viewer paces its render loop with a condition variable (`FramePacer`) and
wakes it from the serve thread the moment a request is queued, so latency is
independent of window focus. A design that instead waited on the GLFW event
loop (`glfwWaitEventsTimeout` + `glfwPostEmptyEvent`) measured ~90x slower
here while backgrounded, because macOS delivers a non-active window's posted
events only at the timeout — which is exactly why this harness exists. Always
click another app so the OID window is **not** frontmost before measuring.

## Usage

Build the viewer, then launch it with the agent endpoint enabled and a buffer
to look at (any image or `.npy`; `gen_image.py` writes a colourful one):

```bash
# from the repo root
python resources/oidmcp/acceptance/gen_image.py /tmp/oid_vis.npy
OID_AGENT=1 build/src/oidwindow --open /tmp/oid_vis.npy &
# now click another app so the OID window is backgrounded
```

Then, from this directory's package environment:

```bash
cd resources/oidmcp

# Quantitative: per-request round-trip latency against the backgrounded window.
# A working wake lands ~1 ms; a dead/throttled wake sits near half a refresh
# period (~8 ms at 60 Hz), which "feels instant" would mask.
uv run python acceptance/latency.py

# Visual: animate the view (rotation + zoom + pan) via set_view; watch the
# backgrounded window render at the monitor rate. Smooth == the paced loop
# renders while inactive.
uv run python acceptance/spin.py --seconds 12
```

Both discover the viewer automatically via the agent discovery directory
(`OID_AGENT_DIR`, or the per-user default), so no pid/port is needed.

## IPC-push path (buffers pushed from the debugger side)

The two scripts above exercise the *agent* path (control calls waking the
render loop). The other half is the *IPC* path: buffers pushed over the
debugger->viewer socket, picked up by the viewer's periodic `ipc.poll()` each
frame rather than by a wake. The repo already ships a check for it -- the
plugin's test mode pushes two sample buffers to a freshly spawned viewer over
the real IPC transport:

```bash
# from an install tree (out/OpenImageDebugger), using the Homebrew python3 on macOS
OID_AGENT=1 python3 out/OpenImageDebugger/oid.py --test
```

A viewer opens showing `sample_buffer_1` (colour pattern) and `sample_buffer_2`
(Mandelbrot). Background it, then drive `spin.py` against it to confirm the
paced loop keeps rendering the IPC-pushed buffer while inactive. Because the
FramePacer keeps the loop iterating at the monitor period regardless of focus,
a debugger-pushed buffer repaints within ~one period (~17 ms at 60 Hz) even
backgrounded -- the same liveness the agent-latency numbers above prove.
