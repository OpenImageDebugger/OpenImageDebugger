# OID VS Code: Editor split viewer — Design

**Date:** 2026-06-27  
**Status:** Approved  
**Parent spec:** `docs/superpowers/specs/2026-06-23-oid-wasm-vscode-design.md`  
**Builds on:** `docs/superpowers/specs/2026-06-27-oid-vscode-live-watch-design.md` (live watch + sidebar `WebviewView`)

**Goal:** On debugger stop, show the OID WASM viewer in an **editor split beside source code** instead of the activity-bar sidebar, so the Run and Debug sidebar can open without hiding the viewer.

---

## 1. Problem

The extension currently registers a `WebviewView` in the left activity bar. On breakpoint:

1. VS Code focuses the **Run and Debug** view in the same primary sidebar.
2. The extension calls `viewer.reveal()` on first stop to show OID.

Both views share one sidebar slot — Debug wins; OID is hidden until the user clicks its activity-bar icon again.

A secondary-sidebar default was attempted earlier but is blocked on Cursor (VS Code API 1.105.1; `secondarySidebar` contribution requires 1.106+).

**Desired UX (approved):** Image **beside source** in an editor split on stop. **Editor only** — remove the sidebar view entirely.

---

## 2. Decision

| Topic | Choice | Rationale |
|-------|--------|-----------|
| Host type | `WebviewPanel` | Native editor-area tab; supports `ViewColumn.Beside` split |
| Open trigger | First `stopped` event per debug session | Same as today; avoids opening before user debugs |
| Focus | `preserveFocus: true` on reveal | Keep keyboard in source while image appears |
| Sidebar view | **Removed** | No competition with Debug; simpler manifest |
| Layout API | `ViewColumn.Beside` | Standard VS Code split beside active editor |
| No active editor | `ViewColumn.Two` fallback | Still show viewer if stop happens with no focused editor |
| WASM lifetime | `retainContextWhenHidden: true` on panel webview | Same intent as current `WebviewView` registration option |
| Session end | Dispose panel | Clean slate per debug session |
| User closes panel mid-session | Recreate on next `ensureReady()` | Plot/open commands still work |
| Configuration | None in v1 | YAGNI; single layout |

**Rejected alternatives:**

- `workbench.action.splitEditorRight` before open — mutates unrelated layout; brittle.
- Fixed `ViewColumn.Two` always — stacks on existing column-2 tabs instead of true beside-active-editor split.
- Keep sidebar + editor — user chose editor-only.

---

## 3. Components (`openimagedebugger-vscode`)

| File | Change | Responsibility |
|------|--------|----------------|
| `package.json` | **modify** | Remove `viewsContainers` / `views`; keep commands and `engines ^1.85.0` |
| `src/webview/panel.ts` | **modify** | `WebviewViewProvider` → singleton `WebviewPanel` owner; `openBeside()`, `dispose()` |
| `src/extension.ts` | **modify** | Drop `registerWebviewViewProvider`; first stop → `openBeside()`; session end → `dispose()` |

No changes to `session-manager.ts`, DAP bridge, or WASM media.

---

## 4. Data flow

```
DebugAdapterTracker: stopped(threadId)
  if (!openedThisSession) { openedThisSession = true; viewer.openBeside() }
  SessionManager.onStopped(threadId)
    └─> sink.ensureReady()  // creates panel if disposed
        └─> sink.sendPlotBuffer(...)

oid.openPanel command
  └─> viewer.openBeside()

debug session terminated
  └─> viewer.dispose(); manager.reset(); openedThisSession = false
```

**`ViewerController.openBeside()`:**

1. If panel exists → `panel.reveal(column, true)` where `column` is `Beside` when an active editor exists, else `Two`.
2. Else → `createWebviewPanel('oid.viewer', 'Open Image Debugger', column, { retainContextWhenHidden: true, enableScripts, localResourceRoots })`, set HTML, wire `viewer-ready` message, register `onDidDispose`.

**`ViewerController.ensureReady()`:** if not ready, call `openBeside()`, then await `viewer-ready` (60s timeout, unchanged).

---

## 5. Error handling

| Case | Behavior |
|------|----------|
| WASM load failure | Existing spinner/error UI in webview HTML |
| `viewer-ready` timeout | `ensureReady()` rejects; session manager swallows per-variable failures on auto-replot |
| Panel disposed mid-await | `onDidDispose` clears ready state; next plot recreates |
| No debug session | Existing command guards unchanged |

---

## 6. Testing

| Layer | What |
|-------|------|
| Unit | Existing `session-manager` tests unchanged (`PlotSink` mock) |
| Compile | `npm run compile` in `openimagedebugger-vscode` |
| Manual | Start CodeLLDB session with `oid.watchOnStop` → hit breakpoint → viewer opens right of source; Debug sidebar on left unaffected; subsequent stops update image; session end closes panel; `OID: Open Viewer Panel` opens split without debugging |

---

## 7. Out of scope

- `oid.viewLocation` setting (sidebar vs editor)
- Dual `secondarySidebar` + editor for VS Code 1.106+ hosts
- Moving Debug panel or suppressing VS Code stop UI
- Editor split column ratio persistence
