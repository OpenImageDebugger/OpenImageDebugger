# OID Editor Split Viewer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the OID viewer from the activity-bar `WebviewView` to a `WebviewPanel` that opens beside the active editor on first debugger stop.

**Architecture:** Refactor `ViewerController` to own a singleton `WebviewPanel` with `ViewColumn.Beside` (fallback `ViewColumn.Two`). Remove sidebar contributions from `package.json` and `registerWebviewViewProvider` from `extension.ts`. Session lifecycle disposes the panel when debugging ends.

**Tech Stack:** TypeScript, VS Code Extension API (`WebviewPanel`, `ViewColumn`), existing WASM loader HTML in `panel.ts`.

**Spec:** `docs/superpowers/specs/2026-06-27-oid-editor-split-view-design.md`

**Repo:** `openimagedebugger-vscode` (sibling of `OpenImageDebugger`)

---

## File map

| File | Action |
|------|--------|
| `openimagedebugger-vscode/package.json` | Remove sidebar `viewsContainers` / `views` |
| `openimagedebugger-vscode/src/webview/panel.ts` | `WebviewView` → `WebviewPanel` |
| `openimagedebugger-vscode/src/extension.ts` | Wire `openBeside` / `dispose`; drop view provider |

---

### Task 1: Remove sidebar manifest entries

**Files:**
- Modify: `openimagedebugger-vscode/package.json`

- [ ] **Step 1: Edit `package.json`**

Remove the entire `viewsContainers` and `views` blocks under `contributes`. The `contributes` section should look like:

```json
  "contributes": {
    "commands": [
      { "command": "oid.openPanel", "title": "OID: Open Viewer Panel" },
      { "command": "oid.plot", "title": "OID: Plot Variable" },
      { "command": "oid.clearWatch", "title": "OID: Clear Live Watches" }
    ],
    "configuration": {
      "title": "Open Image Debugger",
      "properties": {
        "oid.watchOnStop": {
          "type": "array",
          "items": { "type": "string" },
          "default": [],
          "description": "Variable names to plot automatically on every debugger stop."
        }
      }
    }
  },
```

- [ ] **Step 2: Verify JSON**

Run: `node -e "JSON.parse(require('fs').readFileSync('package.json','utf8')); console.log('ok')"`  
Expected: `ok`

---

### Task 2: Refactor ViewerController to WebviewPanel

**Files:**
- Modify: `openimagedebugger-vscode/src/webview/panel.ts`

- [ ] **Step 1: Replace the class at the bottom of `panel.ts`**

Delete `export class ViewerController implements vscode.WebviewViewProvider { ... }` and replace with:

```typescript
function targetColumn(): vscode.ViewColumn {
  return vscode.window.activeTextEditor
    ? vscode.ViewColumn.Beside
    : vscode.ViewColumn.Two;
}

/**
 * Owns the single viewer webview (local WASM) in an editor split and forwards
 * encoded PlotBufferContents frames.
 */
export class ViewerController {
  static readonly panelType = 'oid.viewer';

  private panel: vscode.WebviewPanel | undefined;
  private ready = false;
  private readonly readyWaiters: Array<() => void> = [];

  constructor(private readonly context: vscode.ExtensionContext) {}

  /** Open or focus the viewer beside the active editor without stealing focus. */
  openBeside(): void {
    const column = targetColumn();
    if (this.panel) {
      this.panel.reveal(column, true);
      return;
    }
    const media = vscode.Uri.joinPath(this.context.extensionUri, 'media');
    const panel = vscode.window.createWebviewPanel(
      ViewerController.panelType,
      'Open Image Debugger',
      column,
      {
        retainContextWhenHidden: true,
        enableScripts: true,
        localResourceRoots: [media],
      }
    );
    panel.webview.html = htmlFor(panel.webview, media);
    panel.webview.onDidReceiveMessage((msg) => {
      if (msg?.type === 'oid-control' && msg.event === 'viewer-ready') {
        this.ready = true;
        for (const w of this.readyWaiters.splice(0)) w();
      }
    });
    panel.onDidDispose(() => {
      this.panel = undefined;
      this.ready = false;
      for (const w of this.readyWaiters.splice(0)) w();
    });
    this.panel = panel;
  }

  dispose(): void {
    this.panel?.dispose();
    this.panel = undefined;
    this.ready = false;
    for (const w of this.readyWaiters.splice(0)) w();
  }

  ensureReady(): Promise<void> {
    if (this.ready) return Promise.resolve();
    this.openBeside();
    return new Promise<void>((resolve, reject) => {
      const timer = setTimeout(() => {
        reject(new Error('Viewer did not become ready'));
      }, READY_TIMEOUT_MS);
      this.readyWaiters.push(() => {
        clearTimeout(timer);
        resolve();
      });
    });
  }

  async sendPlotBuffer(bytes: Uint8Array): Promise<void> {
    await this.ensureReady();
    this.panel?.webview.postMessage({ type: 'oid-ipc-forward', payload: Array.from(bytes) });
  }
}
```

- [ ] **Step 2: Update the file header comment**

Replace the old `WebviewView` / secondary sidebar comment block (lines 116–123) — it is removed by the replacement above.

- [ ] **Step 3: Compile**

Run: `cd openimagedebugger-vscode && npm run compile`  
Expected: errors in `extension.ts` until Task 3 (expected temporarily)

---

### Task 3: Wire extension.ts

**Files:**
- Modify: `openimagedebugger-vscode/src/extension.ts`

- [ ] **Step 1: Remove WebviewViewProvider registration**

Delete this block entirely:

```typescript
  context.subscriptions.push(
    vscode.window.registerWebviewViewProvider(ViewerController.viewId, viewer, {
      webviewOptions: { retainContextWhenHidden: true },
    })
  );
```

- [ ] **Step 2: Rename session flag and call `openBeside`**

Change `revealedThisSession` → `openedThisSession` and `viewer.reveal()` → `viewer.openBeside()`:

```typescript
  let openedThisSession = false;
```

Inside the `stopped` handler:

```typescript
              if (!openedThisSession) {
                openedThisSession = true;
                viewer.openBeside();
              }
```

- [ ] **Step 3: Dispose panel on session end**

In `onDidTerminateDebugSession`:

```typescript
    vscode.debug.onDidTerminateDebugSession(() => {
      stoppedThreadId = undefined;
      openedThisSession = false;
      viewer.dispose();
      manager.reset();
    })
```

- [ ] **Step 4: Update openPanel command**

```typescript
    vscode.commands.registerCommand('oid.openPanel', () => viewer.openBeside()),
```

- [ ] **Step 5: Compile and test**

Run: `cd openimagedebugger-vscode && npm run compile && npm test`  
Expected: compile OK, 26 tests pass

- [ ] **Step 6: Commit in `openimagedebugger-vscode`**

```bash
cd openimagedebugger-vscode
git add package.json src/webview/panel.ts src/extension.ts
git commit -m "feat(webview): open viewer in editor split beside source on stop"
```

---

### Task 4: Manual verification

- [ ] **Step 1: Launch Extension Development Host**

Run the extension (F5) from `openimagedebugger-vscode`.

- [ ] **Step 2: Debug smoke test**

1. Configure `oid.watchOnStop` with a known image variable.
2. Start a CodeLLDB debug session and hit a breakpoint.
3. Confirm viewer tab opens **to the right of source**; Run and Debug sidebar can stay open on the left.
4. Continue → stop again; image updates without reloading WASM.
5. End debug session; viewer tab closes.
6. Run command **OID: Open Viewer Panel** without debugging; split opens.

---

## Plan self-review

| Spec requirement | Task |
|------------------|------|
| Remove sidebar view | Task 1 |
| WebviewPanel + Beside | Task 2 |
| First stop opens split | Task 3 |
| Session end dispose | Task 3 |
| preserveFocus | Task 2 `reveal(column, true)` |
| No active editor fallback | Task 2 `targetColumn()` |
| retainContextWhenHidden | Task 2 panel options |
| No new settings | (none added) |

No placeholders. Type names consistent: `openBeside`, `dispose`, `panelType`.
