# WASM Large-Buffer Tiling — Design

**Date:** 2026-06-29
**Status:** Approved (brainstorming)
**Repos:** `OpenImageDebugger` (C++ WASM viewer), `openimagedebugger-vscode` (extension)

## Problem

Plotting a large buffer (e.g. 40000×2160, any dtype) into the WASM viewer
renders **blank** — autocomplete and the rest of the UI work, but no image
appears. The C++ dimension/size limits are generous (`MAX_BUFFER_DIMENSION =
131072`, `MAX_BUFFER_SIZE = 16 GB`), so a 40000×2160 buffer would be accepted
*if the bytes arrived*. The failure is in the **transport**, not the renderer.

A single `PlotBufferContents` frame carrying the full pixel payload hits three
walls at once on the extension→webview hop:

1. **`Array.from(bytes)`** in `panel.ts` (lines 249, 295) boxes every byte into
   a JS array — an ~8–16× memory blow-up plus a giant structured clone.
2. **~64 MB postMessage ceiling** in Chromium (noted in the
   `2026-06-24-oid-wasm-vscode-qt611-design.md`).
3. **`int` length overflow** at 2 GB in the EM_JS transport bindings
   (`oid_send_to_js(ptr, int len)`, `oidEnqueueInbound(ptr, int len)`). Worst
   case (float64×4 at 40000×2160 ≈ 2.76 GB) overflows.

GL texture tiling into 2048² tiles **already exists** in `Buffer::setup_gl_buffer`
and is not the problem. The original design always intended extension-side
tiling (`2026-06-23-oid-wasm-vscode-design.md`, line 117: "Extension tiles large
buffers"); it was deferred and never built.

## Goal

Send large buffers as a sequence of size-bounded chunks so no single message
exceeds the postMessage/​`int` limits, while preserving every viewer feature
(GL tiling, contrast min/max, pixel-value hover). Out of scope: streaming that
avoids ever holding the full buffer (Approach B below) and source-side
downsampling (Approach C). Both are noted as possible future work.

## Approach chosen: A — reassemble in C++

Chunk only the **transport**. The viewer concatenates chunks into one
contiguous buffer in the same owned storage the renderer already uses
(`deps_.buffer_data.held_buffers[name]`), then runs **today's pipeline
unchanged**. The only thing that changes is how bytes get from the extension to
that storage.

- ✅ Directly fixes the blank-render (a transport failure).
- ✅ Minimal C++ logic change; GL tiling, contrast scan, and pixel-hover readout
  are untouched because the assembled buffer is byte-identical to today's.
- ✅ Handles the realistic cases: uint8/uint16/float32 up to ~1–2 GB.
- ⚠️ Bounded by the 32-bit WASM heap, so a ~2.7 GB float64×4 buffer still won't
  fit. Only streaming (B) would fix that; it is the rare case.

### Rejected alternatives

- **B — stream tiles straight to GL, never hold the full buffer.** Scales past
  the heap ceiling, but pixel-value hover (a core OID feature) needs the bytes
  and would require round-tripping reads to the extension; contrast min/max must
  become incremental. Much larger, riskier change. Deferred.
- **C — downsample huge buffers at the source.** Trivial payloads but destroys
  pixel-exact inspection, the whole point of OID. Only viable as an optional
  mode, not the core fix.

## Wire protocol

The extension chooses per buffer based on serialized pixel size vs a shared
`CHUNK_BUDGET` (~16 MB, safely below the 64 MB postMessage ceiling and far from
the 2 GB `int` boundary):

- **Small buffers (≤ budget):** unchanged single `PlotBufferContents` (type 3).
  Desktop/TCP path also untouched.
- **Large buffers (> budget):** a three-message sequence, all keyed by
  `variable_name`:

| Type | Name | Payload |
|---|---|---|
| 10 | `PlotBufferBegin` | variable_name, display_name, pixel_layout, transpose, width, height, channels, stride, type, total_byte_size |
| 11 | `PlotBufferChunk` | variable_name, row_offset, row_count, bytes (`row_count × stride`, tightly packed) |
| 12 | `PlotBufferEnd` | variable_name |

Chunks are **full-width row strips** (`row_count = floor(budget / stride)`), so
each chunk is a contiguous byte range at offset `row_offset × stride` — assembly
is a single `memcpy`, no per-tile scatter. A single 40000-wide float64×4 row is
~1.28 MB, far under budget, so row-strip chunking always fits. New types are
additive (10–12); the existing protocol and the desktop TCP/Python bridge are
unaffected.

## C++ assembly

A small `BufferAssembler` owns in-progress transfers, keyed by name:

- **Begin** → allocate the entry in `held_buffers[name]` sized to
  `total_byte_size`; stash geometry (width/height/channels/stride/type/
  pixel_layout/transpose/display_name).
- **Chunk** → `memcpy` bytes to `offset = row_offset × stride`; bounds-checked
  against `total_byte_size`. Out-of-range or overflowing chunks are rejected
  with a logged error.
- **End** → build `BufferParams` pointing at the now-complete owned buffer, then
  call the **exact existing path** from `decode_plot_buffer_contents`
  (Float64→Float32 conversion, `create_stage`/`initialize` or `buffer_update`,
  `select_stage`, `request_render_update`, image-list + label update). Drop the
  staging entry.

Because assembly lands in the same owned storage the renderer's non-owning
`Buffer::buffer_` span already points into, nothing downstream changes.

`MessageHandler` dispatch gains three cases (10/11/12) routed to the assembler;
everything else is untouched.

## Extension side (TypeScript)

1. **Stop boxing bytes.** In `panel.ts`, replace `Array.from(bytes)` on the
   large-payload sends (lines 249, 295) with the `Uint8Array` directly; the
   inbound bridge's `toUint8Array(...)` already accepts all forms. The small
   viewer→host send (line 57) can switch too but is low-stakes.
2. **Chunked encoder.** Add `buildPlotBufferBegin`, `buildPlotBufferChunk`,
   `buildPlotBufferEnd` to `message-exchange.ts`. The plot sink branches: if
   `pixels.byteLength > CHUNK_BUDGET`, emit Begin + N Chunks (zero-copy
   `subarray` views) + End; else the existing single-frame path. Budget is a
   shared constant.

The DAP read in `dap-client.ts` is left as-is (single `readMemory`). If it
proves to be a second bottleneck — the still-unverified "no console output yet"
unknown — it is an isolated follow-up that does not affect this protocol.

## Defensive cleanup

Widen `oid_send_to_js` / `oidEnqueueInbound` / `PostMessageTransport::send`
length params from `int` to `std::size_t` (`uintptr_t`/`double` across the EM_JS
boundary). Chunk sizes keep us far from the 2 GB line regardless, but this
removes the latent overflow.

## Testing

- **C++ unit — `BufferAssembler`:** Begin/Chunk×N/End reconstructs a known byte
  pattern identical to the single-frame buffer; out-of-order/overflow chunks
  rejected; an interleaved second buffer does not corrupt the first.
- **C++ equivalence:** a buffer sent chunked yields the same `BufferParams`
  (geometry + bytes) as the same buffer sent as one `PlotBufferContents`.
- **Extension unit (`node:test`):** `buildPlotBuffer*` round-trips via the
  existing decoder; the sink picks chunked vs single exactly at the budget
  boundary; chunk count and total byte coverage are exact.
- **Manual:** plot a 40000×2160 buffer of each dtype; confirm it renders, hover
  shows correct pixel values, and contrast works.

## Risks / open questions

- The blank-render root cause is inferred (transport), not yet confirmed from
  console output. If a C++ `[Error]` line *does* appear for large buffers, that
  points at GL/validation instead and would be addressed before/with this work.
  Reproduction + a quick console check is the recommended first implementation
  step.
- A single `readMemory` for hundreds of MB over DAP may be slow or capped; if so,
  chunk the DAP read as a separate follow-up (does not change the wire protocol).
