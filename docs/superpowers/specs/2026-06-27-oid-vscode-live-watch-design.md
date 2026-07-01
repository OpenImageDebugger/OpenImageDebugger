# OID VS Code — Live Watch Set (auto-refresh plotted buffers on stop)

**Date:** 2026-06-27
**Repo:** `openimagedebugger-vscode` (branch `feat/p2-codelldb-bridge`)
**Status:** Approved design

## Problem

A user plots a variable (via `oid.plot`), the image shows once, but it never
updates on subsequent debugger stops. The only auto-update mechanism today is
the `oid.watchOnStop` config list; when it is empty (the common case),
`SessionManager.onStopped()` returns early and nothing is re-plotted.

The user expectation is "plot whatever I'm looking at, and have it keep
updating as I step." The extension currently has no memory of what was
plotted, so it cannot refresh it.

## Decision

Implement a **viewer-independent, extension-owned live set** (Option A). The
extension remembers the variable names it has plotted in the current debug
session and re-plots them on every stop. This ships entirely in the extension:
no C++ change, no WASM rebuild, no redeploy of the hosted GitHub Pages viewer,
and it does not touch the fragile viewer→host IPC path (so the
`MessageComposer::send` per-block fragmentation issue is irrelevant here).

The fully viewer-authoritative model (querying the viewer's open buffers via
`GetObservedSymbols` on each stop) is deliberately deferred — see *Deferred*.

## Design

### State & ownership
`SessionManager` owns a per-session live set:

```ts
private readonly liveNames = new Set<string>();
```

The **effective re-plot set** on each stop is `liveNames ∪ watchOnStop` — the
dynamic set the user builds by plotting, unioned with the static config list
(which keeps working unchanged). The set is cleared when the debug session
terminates, so each session starts fresh.

### Behavior

| Trigger | Behavior |
|---|---|
| `plotVariable(name, threadId)` (manual `oid.plot`) | Plot once. On **success**, add `name` to `liveNames`. On **failure**, warn (as today) and do **not** enroll. |
| `onStopped(threadId)` | Resolve top frame, re-plot every name in the effective set. Failures are **silent** (no toast) and the name is **retained**, so a momentarily out-of-scope variable resumes updating when scope returns. Empty effective set → no-op. |
| `reset()` | Clear `liveNames`. Called from `onDidTerminateDebugSession`. |
| Overlap | If an `onStopped` re-plot is still in flight when the next stop arrives (rapid stepping), skip the new run rather than piling up DAP requests. |

### Commands & config
- `oid.plot` — unchanged entry point; now also enrolls the variable into the
  live set via `plotVariable`.
- **New** `oid.clearWatch` ("OID: Clear Live Watches") — empties `liveNames`
  (leaves the `watchOnStop` config untouched). Provides the "stop watching"
  affordance for the staleness caveat below.
- `oid.watchOnStop` config — unchanged; now unioned into the effective set.

### Error handling rationale
Auto-replot failures are silent + retained (not dropped) because a variable
commonly leaves and re-enters scope while stepping; dropping on first failure
would lose it permanently. Manual plot keeps the existing warning toast so the
user gets feedback when an explicit action fails.

## Caveat (accepted)
The extension is the source of truth, not the viewer. If the user closes a
buffer *inside the viewer UI*, the extension does not know and keeps refreshing
it. `oid.clearWatch` is the escape hatch. Precise viewer-side open/close
tracking is the deferred Option B.

## Testing (TDD)
Extend `test/session-manager.test.ts`:
- `plotVariable` success enrolls the name → a later `onStopped` re-plots it.
- `plotVariable` failure does **not** enroll.
- `onStopped` re-plots the union of live set + watch list, deduped.
- `onStopped` failure is silent and retains the name.
- Overlap guard: a second `onStopped` while one is in flight is skipped.
- `reset()` clears the set.

## Deferred — Option B (viewer-authoritative)
On each stop, query the viewer's open buffers via `GetObservedSymbols` and
re-plot exactly those. Requires fixing the viewer→host fragmentation
(`MessageComposer::send(ITransport&)` in `src/ipc/message_exchange.h` coalescing
all blocks into one `transport.send` so each logical frame is one postMessage),
rebuilding the WASM, and redeploying it to GitHub Pages. Layers on top of this
design later.
