# OID VS Code P3 Refine: Viewer-owned session loop — Design

**Date:** 2026-06-27  
**Status:** Approved  
**Parent spec:** `docs/superpowers/specs/2026-06-24-oid-vscode-p3-session-design.md`  
**Supersedes (watch ownership):** `docs/superpowers/specs/2026-06-27-oid-vscode-live-watch-design.md`  
**Builds on:** P2 CodeLLDB bridge, editor-split viewer (`2026-06-27-oid-editor-split-view-design.md`)

**Goal:** Complete the original P3 session loop — WASM autocomplete from in-scope symbols, add variables from the viewer UI, and viewer-owned auto-replot on stop — replacing the interim extension-owned `liveNames` model.

---

## 1. Problem

After P2 and live-watch shipping:

| Capability | Status |
|------------|--------|
| Plot via command palette (`oid.plot`) | Works |
| Auto-replot on stop | Works via extension-owned `liveNames ∪ watchOnStop` |
| WASM symbol autocomplete (`SetAvailableSymbols`) | **Missing** — never sent |
| Add variable from WASM symbol box (`PlotBufferRequest`) | **Missing** — `oid-ipc-out` not handled |
| Viewer-owned watch set (`GetObservedSymbols` poll) | **Missing** — live-watch deferred this |

Root cause for viewer→host IPC: `MessageComposer::send(ITransport&)` calls `transport.send()` once per internal block. TCP desktop treats this as one byte stream; **postMessage sends each block as a separate JS message**, so the extension receives fragmented frames and cannot `decodeInbound` them.

Host→viewer messages built in TypeScript (`buildPlotBufferContents`, `buildSetAvailableSymbols`) are already single `Uint8Array` frames and work.

---

## 2. Decisions

| Topic | Choice | Rationale |
|-------|--------|-----------|
| Watch source of truth | **Viewer-owned** (original P3) | Desktop parity; remove in viewer UI respected on next stop |
| Replace live-watch | **Yes** — remove `liveNames` | User chose full P3 model |
| `oid.plot` | Same as `PlotBufferRequest` — plot at frame; viewer observes via `PlotBufferContents` | One code path |
| `oid.watchOnStop` | One-time seed on first stop: `observed ∪ watchList` | Original P3 §4 |
| `oid.clearWatch` | **Remove command** | Viewer owns set; user removes buffers in UI |
| Symbol enumeration | Local + Arguments scopes, current frame, unfiltered | Original P3 |
| IPC fragmentation fix | **Coalesce in `MessageComposer::send(ITransport&)`** (OID repo) | One fix for all viewer→host messages; TCP-safe |
| WASM delivery | Rebuild; copy artifacts to `openimagedebugger-vscode/media/` | Local WASM already used (not GitHub Pages) |
| `GetObservedSymbols` timeout | 1000 ms → `[]` + warn | Original P3 §5; stop loop never hangs |
| Cross-restart persistence | Deferred P5 | Unchanged |
| OID UI parity (auto-contrast, etc.) | Deferred P4 | Unchanged |

**Rejected:**

- Extension-only chunk reassembly in `panel.ts` — duplicates framing; fragile.
- JS coalesce in `window.oidSend` — bypasses C++ transport; doesn't help desktop.

---

## 3. Architecture

### Stop loop (replaces `liveNames` replot)

```
DebugAdapterTracker: stopped(threadId)
  └─> SessionManager.onStopped(threadId)
        frameId = resolveFrame(threadId)
        1. syms = listScopeSymbols(frameId)
           channel.setAvailableSymbols(syms.map(s => s.name))
        2. observed = await channel.getObservedSymbols()
        3. if (!seeded) { observed = union(observed, getWatchList()); seeded = true }
        4. for name in observed:
             try plot → PlotBufferContents
             catch warn (per-name); continue
```

### Viewer-initiated add (any time while paused)

```
oid-ipc-out → decodeInbound → PlotBufferRequest(name)
  └─> SessionManager.onPlotBufferRequest(name, stoppedThreadId)
        └─> plotAtFrame (warn on error)
```

### Session end

```
onDidTerminateDebugSession → viewer.dispose(); manager.reset()  // reset clears seeded only
```

---

## 4. Components

### OpenImageDebugger

| File | Change |
|------|--------|
| `src/ipc/message_exchange.h` | `MessageComposer::send(ITransport&)` concatenates blocks → single `transport.send` |
| `tests/test_message_exchange.cpp` | Assert mock transport receives one send with expected total length for type+string message |

Rebuild WASM (`oidwindow.wasm`, `oidwindow.js`); copy into extension `media/`.

### openimagedebugger-vscode

| File | Change |
|------|--------|
| `debugger/symbol-enumerator.ts` | **new** — `listScopeSymbols(session, frameId): Promise<{name,type}[]>` |
| `debugger/dap-client.ts` | **extend** — `scopes(frameId)`, `variables(ref)` if not present |
| `ipc/message-exchange.ts` | No API change; ensure tests cover types 0,1,2,4 |
| `webview/panel.ts` | **ViewerChannel**: `setAvailableSymbols`, `getObservedSymbols`, handle `oid-ipc-out`, `onPlotBufferRequest` callback |
| `session/session-manager.ts` | P3 loop; remove `liveNames`; `seeded` flag; `onPlotBufferRequest` |
| `extension.ts` | Wire channel, enumerator, inbound plot requests; remove `oid.clearWatch` |
| `package.json` | Remove `oid.clearWatch` command contribution |
| `test/session-manager.test.ts` | Rewrite for viewer-owned mocks |
| `test/symbol-enumerator.test.ts` | **new** |

---

## 5. ViewerChannel (`panel.ts`)

**Outbound:** `postMessage({ type: 'oid-ipc-forward', payload })` — unchanged.

**Inbound:** On `oid-ipc-out`, `decodeInbound(payload)`:

- `{ kind: 'observedSymbols', names }` → resolve pending `getObservedSymbols()` promise
- `{ kind: 'plotRequest', name }` → invoke `onPlotBufferRequest(name)`
- `{ kind: 'unknown' }` → ignore

**`getObservedSymbols()`:** send `buildGetObservedSymbols()`; await type-1; 1000 ms timeout → `[]` and `console.warn`. At most one pending poll; supersede previous with `[]`.

**`setAvailableSymbols(names)`:** send `buildSetAvailableSymbols(names, 'wasm32')` after `ensureReady()`.

---

## 6. Error handling

| Failure | Behavior |
|---------|----------|
| Scope enumeration fails | Skip `SetAvailableSymbols`; warn; continue poll + plot |
| `GetObservedSymbols` timeout | `[]`; no replot this stop |
| `PlotBufferRequest` / `oid.plot` failure | Status-bar warning; no crash |
| Per-buffer plot failure on stop | Warn per name; continue loop |
| No stopped thread for inbound request | Ignore or warn "pause first" |
| Panel not ready | `ensureReady()` opens editor split (existing) |

---

## 7. Testing

**OID C++:** composer coalescing sends exactly one transport call with concatenated bytes.

**Extension unit:**

- Symbol enumerator filters scopes (Local, Arguments only).
- Session manager: `setAvailableSymbols` called with enumerated names; poll observed; first-stop seed; one plot per observed name; `onPlotBufferRequest` plots; failures isolated.
- Remove live-watch-specific tests (`liveNames` enroll, `reset` clears names).

**Manual (original P3 §7):**

1. Break in testbench `nest()` — autocomplete lists locals + arguments.
2. Type `TestField` in viewer symbol box — plots.
3. `oid.watchOnStop: ["TestField"]` — auto-plots first stop.
4. Step/continue — watched buffers refresh.
5. Remove buffer in viewer — next stop does not replot it.

---

## 8. Phase boundary

**In this refine:**

- MessageComposer coalescing + WASM refresh
- Full P3 extension wiring
- Remove extension-owned live set and `oid.clearWatch`

**Still deferred:** P5 persistence; P4 UI parity; OBP protobuf; no-copy transfer.

---

## 9. Self-review

- **Placeholders:** None.
- **Consistency:** Viewer-owned decision matches stop loop, removes `liveNames`, removes `oid.clearWatch`. Coalescing fix enables inbound decode path assumed in §5.
- **Scope:** Two repos, bounded files; single implementation plan.
- **Ambiguity:** `oid.plot` does not maintain a separate extension set — viewer is authoritative after plot. `seeded` is per debug session only.
