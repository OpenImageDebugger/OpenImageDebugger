# OID VS Code P3: Session management — symbol list, watched buffers, auto-update — Design

**Date:** 2026-06-24
**Status:** Approved design
**Parent spec:** `docs/superpowers/specs/2026-06-23-oid-wasm-vscode-design.md` (phase P3)
**Builds on:** `docs/superpowers/specs/2026-06-24-oid-vscode-p2-codelldb-design.md` (P2: CodeLLDB → WASM bridge)

**Goal:** Turn P2's one-shot plot into a live session loop. On every debugger stop the extension populates the viewer's symbol autocomplete, asks the viewer which buffers it is watching, and re-plots them with fresh data. The user can add a buffer by typing its name in the viewer itself. The watch list is **owned by the viewer** (desktop OID parity), seeded once from the `oid.watchOnStop` config.

---

## 1. Context: already exists (after P2)

**OID viewer (`oidwindow`, WASM):** Compiles the **full** desktop `main_window` UI — `symbol_completer`, `symbol_search_input`, the `available_vars`-driven autocomplete, and `message_handler` which already speaks the complete protocol:

| Type | Direction | Meaning |
|------|-----------|---------|
| `GetObservedSymbols = 0` | host → viewer | "Which buffers are you watching?" |
| `GetObservedSymbolsResponse = 1` | viewer → host | list of watched buffer names |
| `SetAvailableSymbols = 2` | host → viewer | populate autocomplete with in-scope names |
| `PlotBufferContents = 3` | host → viewer | pixel data (P2 already uses this) |
| `PlotBufferRequest = 4` | viewer → host | "plot the buffer named X" (user typed/picked it) |

A buffer becomes "observed" in the viewer simply by being plotted (`PlotBufferContents`); `GetObservedSymbols` returns whatever the viewer currently shows. This is the desktop loop, and it requires **no OID repo changes** for P3.

**Extension (`openimagedebugger-vscode`, after P2):**
- `debugger/dap-client.ts` — `evaluate`, `readMemory`, resolve stopped frame.
- `debugger/codelldb-bridge.ts` — `getBufferMetadata(variable, frameId)` → `PlotBufferParams`.
- `typebridge/opencv-mat.ts`, `typebridge/buffer-metadata.ts` — pure decode.
- `ipc/message-exchange.ts` — encodes `PlotBufferContents`; `PlotBufferParams` type.
- `session/session-manager.ts` — transport-agnostic. Current `SessionDeps`: `source` (`getBufferMetadata`), `sink` (`ensureReady`/`sendPlotBuffer`), `encode`, `resolveFrame`, `getWatchList`, `warn`. `onStopped(threadId)` iterates the static `getWatchList()` config.
- `webview/panel.ts` — owns the webview/transport. Outbound: `oid-ipc-forward` → `window.oidOnMessage`. Inbound: `window.oidSend` → `oid-ipc-out` post back to the extension.

---

## 2. Design decisions

| Topic | Choice | Rationale |
|-------|--------|-----------|
| Watch source of truth | **Viewer-owned** | Desktop parity; enables in-viewer add/remove and `PlotBufferRequest`. `oid.watchOnStop` becomes a one-time seed. |
| Sync mechanism | **Poll on stop** (`GetObservedSymbols` request/response), no event-push | Covers add (immediate via `PlotBufferRequest`) and remove (next stop's poll). Event-push would only react to mid-pause *removal*, which nothing needs — YAGNI. |
| OID repo changes | **None** | Existing viewer protocol is sufficient. Reserved only if a concrete gap appears. |
| Symbol enumeration | **Local + Arguments scopes, current frame** | Matches desktop "variables in scope"; plottability checked on plot, not pre-filtered. |
| Cross-restart persistence | **Deferred to P5** | Parent phase table assigns "session persistence" to P5. P3's observed set is in-memory for the live session. |
| Scope boundary | **Extension-only** | No OID C++/WASM changes. |

---

## 3. Components (all in `openimagedebugger-vscode/src`)

| File | Change | Responsibility |
|------|--------|----------------|
| `debugger/symbol-enumerator.ts` | **new** | `listScopeSymbols(frameId)`: DAP `scopes(frameId)` → keep scopes named/typed Local or Arguments → `variables(ref)` → flat `{ name, type }[]`. Only file beyond `dap-client` touching DAP scope/variables. |
| `ipc/message-exchange.ts` | **extend** | `encodeSetAvailableSymbols(names)` (type 2: count + length-prefixed strings, matching `MessageComposer`); `encodeGetObservedSymbols()` (type 0, header-only); `decodeInbound(bytes)` → discriminated union `{ kind: 'observedSymbols', names } \| { kind: 'plotRequest', name }` for types 1 and 4. |
| `webview/panel.ts` | **extend** | Implement a `ViewerChannel`: `setAvailableSymbols(names)`, `getObservedSymbols(): Promise<string[]>` (send type 0, await correlated type-1 reply with timeout), and route inbound `oid-ipc-out` frames through `decodeInbound` → resolve the pending observed-symbols promise or invoke the `onPlotBufferRequest` callback. |
| `session/session-manager.ts` | **extend** | Replace the static-list `onStopped` with the full loop (§4). Extend `SessionDeps` with `listScopeSymbols(frameId)`, and a `channel` exposing `setAvailableSymbols` + `getObservedSymbols` (the existing `sink` stays for plotting). Add `onPlotBufferRequest(name, threadId)` → reuse `plotVariable`. Track an in-memory `seeded` flag. |
| `extension.ts` | **wire** | Build the channel/enumerator deps; on `stopped` call `onStopped`; register the channel's `onPlotBufferRequest` → `SessionManager`; dispose on session end. |

The viewer-coupled correlation/decoding lives in `panel.ts` (the `ViewerChannel`); `session-manager.ts` stays VS-Code-free and unit-testable with mocks, preserving P2's separation.

---

## 4. Data flow

```
DebugAdapterTracker: stopped(threadId)
  └─> SessionManager.onStopped(threadId)
        frameId = resolveFrame(threadId)
        1. syms = listScopeSymbols(frameId)                  // [{name,type}], Local+Arguments
           channel.setAvailableSymbols(syms.map(s => s.name)) // → SetAvailableSymbols (autocomplete)
        2. observed = await channel.getObservedSymbols()      // GetObservedSymbols → Response (timeout → [])
        3. if (!seeded) { observed = union(observed, getWatchList()); seeded = true }
        4. for name in observed:
             try { meta = source.getBufferMetadata(name, frameId)
                   sink.ensureReady(); sink.sendPlotBuffer(encode(meta)) }  // → PlotBufferContents
             catch { warn("could not plot 'name'"); continue }              // P2 error policy

Inbound (any time the viewer user adds a symbol):
  oid-ipc-out → decodeInbound → { kind:'plotRequest', name }
    └─> SessionManager.onPlotBufferRequest(name, currentThreadId)
          → plotVariable(name, threadId)   // fetch + plot; viewer now lists it as observed
```

Add is immediate (`PlotBufferRequest`); remove is reflected at the next stop (poll returns the smaller set). On the first stop after a session/panel opens, the `oid.watchOnStop` names are unioned into the observed set and plotted, which makes the viewer start observing them.

---

## 5. Request/response correlation

`getObservedSymbols()` stores a single `pendingObservedSymbols` resolver on the channel (at most one poll in flight per stop). An inbound type-1 frame resolves it with the decoded names. A 1000 ms timeout resolves it to `[]` and logs a warning — guarding against an old/loading viewer that never replies, so the stop loop never hangs. If a poll is issued while one is pending (defensive), the previous resolver is settled with `[]` first.

---

## 6. Error handling (extends P2 §6)

| Failure | Behavior |
|---------|----------|
| `scopes`/`variables` enumeration fails | Skip `SetAvailableSymbols` this stop; warn; continue to plot loop |
| `GetObservedSymbols` times out / no reply | Resolve `[]`; plot loop is a no-op this stop |
| `PlotBufferRequest` for unknown/out-of-scope name | P2: status-bar warning naming the variable; no crash |
| Per-buffer metadata / `readMemory` failure | P2: skip that buffer; loop continues |
| `stopped` before panel/`viewer-ready` | P2: open panel, queue, replay on ready |

A failing entry never aborts the loop; `onStopped` always processes every observed name.

---

## 7. Testing

**Unit (no debug session):**
- `symbol-enumerator.test.ts` — mocked DAP `scopes`/`variables` fixture → flat `{name,type}` list; verifies Local+Arguments scopes kept and others (Registers/Static) dropped.
- `message-exchange.test.ts` (extend) — `encodeSetAvailableSymbols` and `encodeGetObservedSymbols` byte-match `MessageComposer` fixtures; `decodeInbound` parses type-1 and type-4 frames (incl. empty observed list, multi-name).
- `session-manager.test.ts` — mocked `channel` + `source`: asserts (a) `setAvailableSymbols` sent with enumerated names; (b) observed set polled; (c) first-stop seed = union(observed, watchList) and `seeded` flag flips; (d) one `sendPlotBuffer` per observed name; (e) a throwing `getBufferMetadata` skips only that name; (f) `onPlotBufferRequest` calls `plotVariable`.
- Channel correlation test — `getObservedSymbols()` resolves on inbound type-1, resolves `[]` on timeout.

**Manual (verification):**
1. Build OID testbench; launch CodeLLDB from extension dev host.
2. Break in `testbench/main.cpp` `nest()`. Confirm the viewer's autocomplete lists `nest()`'s locals + arguments.
3. Type `TestField` in the viewer's symbol box → confirm it plots (viewer-initiated `PlotBufferRequest`).
4. Set `oid.watchOnStop: ["TestField"]`, restart → confirm it auto-plots on the first stop.
5. Step/continue → confirm watched buffers re-plot with updated data.
6. Remove `TestField` in the viewer → step → confirm it is **not** re-plotted.

---

## 8. Delivery / phase boundary

- **In P3:** symbol enumeration → `SetAvailableSymbols`; viewer-owned observed set via `GetObservedSymbols` poll; auto-replot on stop; `PlotBufferRequest` inbound; `oid.watchOnStop` first-stop seed.
- **Deferred:** cross-restart session persistence → **P5**; multi-panel / multi-buffer-layout management → later; OBP protobuf migration, no-copy pixel transfer → later; UI parity features (auto-contrast, rotation, go-to, link views) → **P4**.

---

## 9. Self-review (spec)

- **Placeholder scan:** No TBD. Every component names concrete files and the message types it touches.
- **Internal consistency:** Message-type numbers (§1) match encode/decode duties (§3) and the data flow (§4). Viewer-owned decision (§2) is realized by the poll + seed loop (§4) and the deferral of persistence to P5 (§2, §8).
- **Scope check:** Single implementation-plan scope — one new file, three extended files, one wiring change; extension-only.
- **Ambiguity check:** Watch source of truth is the viewer; `oid.watchOnStop` is a one-time seed, not an ongoing source. Symbol enumeration is current-frame Local+Arguments, unfiltered by plottability. `GetObservedSymbols` is request/response polled on stop, not event-pushed. Persistence is explicitly out of P3.
