# OID VS Code P2: CodeLLDB → WASM viewer bridge — Design

**Date:** 2026-06-24
**Status:** Approved for implementation
**Supersedes/refines:** `docs/superpowers/plans/2026-06-24-oid-vscode-p2-codelldb.md`
**Parent spec:** `docs/superpowers/specs/2026-06-23-oid-wasm-vscode-design.md` (phase P2)

**Goal:** Plot a live OpenCV `cv::Mat` from a CodeLLDB debug session into the existing WASM viewer, replacing the hardcoded smoke-test buffer with a real CodeLLDB → metadata → pixels → IPC pipeline. P2 is **extension-only**: no OpenImageDebugger (OID) C++/WASM repo changes.

---

## 1. Context: what already exists

P1 is done. Establishing the actual state (not the P1 spec's intent) is essential because the implementation diverged from the spec on the wire format.

**OID repo (`OpenImageDebugger`):** The Qt6 WASM `oidwindow` builds, loads in a webview, and renders. It is driven by the **legacy binary `MessageComposer` format** (`PlotBufferContents`, message type `3`) over `postMessage`, **not** the OBP v1 protobuf the P1 spec called "the sole wire format." The viewer signals readiness via an `oid-control` / `viewer-ready` message and consumes inbound IPC frames through `window.oidOnMessage(Uint8Array)`.

**Extension repo (`openimagedebugger-vscode`, sibling of the OID repo):** Already scaffolded with a working smoke-test loop:

- `src/extension.ts` — registers `oid.openPanel` only.
- `src/webview/panel.ts` — creates the webview, inlines the loader HTML, and on `viewer-ready` pushes a hardcoded 2×2 RGB buffer encoded by `buildPlotBufferContents` → `oid-ipc-forward` → `oidOnMessage`.
- `src/ipc/message-exchange.ts` — TypeScript port of `buildPlotBufferContents` with a `wasm32` length mode (`MessageType.PlotBufferContents = 3`).
- `src/typebridge/`, `src/debugger/`, `src/session/` — empty stubs.

**postMessage contract (already in use, P2 keeps it):**

| Direction | Message | Meaning |
|-----------|---------|---------|
| webview → extension | `{ type: 'oid-control', event: 'viewer-ready' }` | Viewer ready to receive buffers |
| extension → webview | `{ type: 'oid-ipc-forward', payload: number[] }` | Encoded `PlotBufferContents` frame |
| webview → extension | `{ type: 'oid-ipc-out', payload: number[] }` | Viewer → host IPC (unused in P2) |

**P2's actual job:** swap the hardcoded smoke-test buffer for a real `cv::Mat` read.

---

## 2. Decisions

| Topic | Choice | Rationale |
|-------|--------|-----------|
| Metadata read | CodeLLDB DAP `evaluate` expressions | Fewest round-trips; mirrors the Python bridge's field logic directly; `data` address comes from the evaluate result's `memoryReference`. |
| Wire format | Legacy binary `PlotBufferContents` (`wasm32` mode) | Smallest diff to a working pipeline; matches what the WASM viewer already consumes. OBP protobuf is **deferred**, not pursued in P2. |
| Pixel transfer | Existing `number[]`-through-`postMessage` copy | Known-inefficient but functional; no-copy/transferable path deferred (P2+). |
| Trigger | `oid.plot` command **and** `oid.watchOnStop` config | `oid.plot` for ad-hoc inspection; `oid.watchOnStop` re-plots named buffers on every stop. Matches desktop OID feel. |
| Scope boundary | Extension-only | No OID repo C++/WASM changes in P2. |
| Verification | Full Mat type map ported; **uint8-first** against the testbench | Type decode is cheap to port fully; verification starts with uint8, then float32. |

---

## 3. Components (all in `openimagedebugger-vscode/src`)

| File | Responsibility | Ports from (OID `resources/oidscripts`) |
|------|----------------|------------------------------------------|
| `typebridge/buffer-metadata.ts` | `OID_TYPES_*` enum, `bytesPerChannel(type)`, stride/size helpers. Pure functions. | `sysinfo.py`, `symbols.py` |
| `typebridge/opencv-mat.ts` | Pure flags-decode: given raw field values, produce `PlotBufferParams` **minus pixels**. No DAP coupling. | `oidtypes/opencv.py` (`Mat`) |
| `debugger/dap-client.ts` | Thin DAP wrappers over the active session: `evaluate(expr, frameId)`, `readMemory(memoryReference, count)`, resolve current stopped `threadId`/`frameId`. | `debuggers/lldbbridge.py` (DAP subset) |
| `debugger/codelldb-bridge.ts` | Orchestrates one plot: resolve variable expression → `evaluate` fields → `opencv-mat` decode → compute byte count → `readMemory` → assemble full `PlotBufferParams`. | `lldbbridge.get_buffer_metadata` |
| `session/session-manager.ts` | `DebugAdapterTracker` for debug type `lldb`; caches stopped `threadId`/`frameId`; runs the `oid.watchOnStop` list on `stopped`; exposes `plotVariable(name)`. | `OpenImageDebuggerEvents.stop_handler` |
| `webview/panel.ts` (modify) | Extract `ViewerController.sendPlotBuffer(params)`; make the panel a singleton; move the smoke-test behind a default-off flag. | — |
| `extension.ts` (modify) | Register `oid.plot` / `oid.openPanel`; read `oid.watchOnStop`; wire `SessionManager` ↔ `ViewerController`. | — |

Each unit has one clear purpose. `opencv-mat.ts` and `buffer-metadata.ts` are pure and unit-testable without a debug session. `dap-client.ts` is the only unit touching the VS Code Debug API, so the bridge can be tested against a mock.

---

## 4. cv::Mat metadata extraction (port of `opencv.py`)

OpenCV constants: `CV_CN_SHIFT = 3`, `CV_CN_MAX = 512`, `CV_MAT_CN_MASK = ((CV_CN_MAX - 1) << CV_CN_SHIFT)`, `CV_DEPTH_MAX = (1 << CV_CN_SHIFT)`, `CV_MAT_TYPE_MASK = (CV_DEPTH_MAX * CV_CN_MAX - 1)`.

For a Mat variable expression `M`:

```text
flags  = evaluate "M.flags"          (int)
cols   = evaluate "M.cols"           (int)  → width
rows   = evaluate "M.rows"           (int)  → height
step0  = evaluate "M.step.buf[0]"    (int)  → row byte stride
dataRef = evaluate("M.data").memoryReference

channels   = ((flags & CV_MAT_CN_MASK) >> CV_CN_SHIFT) + 1
type_value = (flags & CV_MAT_TYPE_MASK) & 7        → OID buffer type
row_stride = step0 / channels                       (elements per row)
  if type_value in {UINT16, INT16}:  row_stride /= 2
  if type_value in {INT32, FLOAT32}: row_stride /= 4
  if type_value == FLOAT64:          row_stride /= 8
pixel_layout = channels >= 3 ? 'bgra' : 'rgba'

byteCount = rows * step0
pixels    = readMemory(dataRef, byteCount)   // base64 → Uint8Array
```

OID buffer-type values (`symbols.py`): `UINT8=0, FLOAT32=1, UINT16=2, INT16=3, INT32=4, FLOAT64=6`. The resulting `PlotBufferParams` is `{ variableName, displayName, pixelLayout, transpose:false, width:cols, height:rows, channels, stride:row_stride, bufferType:type_value, pixels }`.

The testbench (`testbench/main.cpp`) defines a faithful `cv::Mat` shape — `data`, `cols`, `rows`, `flags`, `step.buf[2]` with `step.buf[0] = cols*channels*sizeof(T)`, `step.buf[1] = channels` — so these expressions resolve against it.

---

## 5. Data flow

```text
CodeLLDB `stopped`
  → DebugAdapterTracker caches threadId + top frameId
  → SessionManager.onStopped():
       for each name in oid.watchOnStop:  plotVariable(name)

oid.plot command (symbol under cursor, else input box)
  → if no stopped frame: warn "Pause in a debug session first"
  → else plotVariable(name)

plotVariable(name):
  CodeLldbBridge.getBufferMetadata(name)      // §4
  → buildPlotBufferContents(params, 'wasm32') // existing encoder
  → ViewerController.sendPlotBuffer(bytes)
  → panel.webview.postMessage({type:'oid-ipc-forward', payload:[...bytes]})
  → webview oidOnMessage(Uint8Array)          // viewer renders
```

If the panel is not yet open or `viewer-ready` has not fired, `ViewerController` opens the panel and queues the buffer until `viewer-ready`.

---

## 6. Error handling

| Failure | Behavior |
|---------|----------|
| Variable not found / not a `cv::Mat` | Status-bar warning; skip this name (don't abort other watches) |
| Null `data` pointer (`0x0`) | Error notification naming the variable |
| `evaluate` fails | Error naming the variable; skip |
| `readMemory` fails | Retry once; then error |
| `oid.plot` with no stopped frame | "Pause in a debug session first" |
| Panel not ready / `viewer-ready` not seen | Open panel; queue buffer until ready |

A failing watch entry never blocks the others; `onStopped` continues the loop.

---

## 7. Testing

**Unit (no debug session):**
- `buffer-metadata.test.ts` — `bytesPerChannel` / stride helpers across all OID types.
- `opencv-mat.test.ts` — flags-decode for CV_8UC3, CV_8UC1, CV_32FC1, CV_16UC1 (channels, type_value, row_stride, pixel_layout) via fixtures.
- `codelldb-bridge.test.ts` — `getBufferMetadata` orchestration against a **mocked DAP client** returning canned `evaluate` + `readMemory` responses (fixture `test/fixtures/cv-mat-variables.json`).
- Keep existing `message-exchange.test.ts`.

**Manual (verification):**
1. Build the OID testbench; launch CodeLLDB from the extension dev host (`.vscode/launch.json`).
2. Break in `testbench/main.cpp` `nest()`.
3. `OID: Plot Variable` on `TestField`; confirm the image renders in the viewer panel.
4. Set `oid.watchOnStop: ["TestField"]`; step/continue; confirm auto-replot on the next stop.
5. Repeat uint8 first, then a float32 Mat.

---

## 8. Scope / deferred (P2+)

Out of P2 by decision: OBP protobuf migration; no-copy / transferable pixel transfer; chunking buffers > 64 MB; `PlotBufferRequest` (WASM → extension) round-trip; CvMat / IplImage / Eigen types; symbol-autocomplete list (P3 — `SetAvailableSymbols`); multi-panel / multi-buffer UI management.

---

## Self-review (spec)

- **Placeholder scan:** No TBD sections; all field expressions, constants, type values, and file responsibilities are explicit.
- **Internal consistency:** §1 contract, §4 extraction, and §5 flow use the same message names (`oid-control`/`viewer-ready`, `oid-ipc-forward`) and the same `PlotBufferParams` shape as the existing `buildPlotBufferContents`. Decisions in §2 are reflected in every later section.
- **Scope check:** Single implementation-plan scope — seven focused files, extension-only, no OID repo changes.
- **Ambiguity check:** Wire format is legacy binary (not OBP) — stated explicitly to override the parent spec's intent. Verification is uint8-first with the full type map ported. `transpose` is always `false` in P2.
