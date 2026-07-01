# OID VS Code Live Watch Set Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make plotted image buffers auto-refresh on every debugger stop by having the extension remember what it plotted in the current session and re-plot that set.

**Architecture:** `SessionManager` (in the `openimagedebugger-vscode` repo) gains a per-session live set of variable names. `plotVariable` enrolls a name on successful plot; `onStopped` re-plots the union of the live set and the `oid.watchOnStop` config, silently. A new `oid.clearWatch` command and a session-terminate hook clear the set. No C++/WASM/hosted-viewer changes.

**Tech Stack:** TypeScript (CommonJS, Node16 module resolution — **all relative imports use `.js` extensions**), VS Code Extension API, `node:test` + `node:assert` run against compiled JS in `out/`.

## Global Constraints

- Repo: `openimagedebugger-vscode`, branch `feat/p2-codelldb-bridge`. (Plan/spec docs live in the `OpenImageDebugger` repo.)
- Relative imports MUST use `.js` extensions (Node16 resolution).
- Tests run against compiled output: `npm run compile` then `npm test` (`node --test out/test/*.js`). Stale `out/` causes phantom passes — start each verification with `rm -rf out`.
- Commit messages: plain, no `Co-Authored-By` / AI-attribution trailer.
- VS Code engine floor stays `^1.85.0`. No new dependencies.

---

### Task 1: Live set — enroll on plot, re-plot union on stop, silent failures, reset

**Files:**
- Modify: `src/session/session-manager.ts` (replace the `SessionManager` class body)
- Test: `test/session-manager.test.ts` (update one existing test, add new tests)

**Interfaces:**
- Consumes: existing `SessionDeps` (`source.getBufferMetadata`, `sink.ensureReady`, `sink.sendPlotBuffer`, `encode`, `resolveFrame`, `getWatchList`, `warn`) — unchanged.
- Produces:
  - `SessionManager.plotVariable(name: string, threadId: number): Promise<void>` — plots once; enrolls `name` into the live set only on success.
  - `SessionManager.onStopped(threadId: number): Promise<void>` — re-plots the deduped union of the live set and `getWatchList()`; failures are silent and names are retained; no-op when the union is empty.
  - `SessionManager.reset(): void` — clears the live set.

- [ ] **Step 1: Update the existing watch test for silent auto-replot and add new tests**

In `test/session-manager.test.ts`, REPLACE the existing test `onStopped plots every watch entry and isolates failures` with the silent version below, then APPEND the new tests after it.

```ts
test('onStopped plots every watch entry and is silent on failure', async () => {
  const { sent, warnings, mgr } = harness({
    getWatchList: () => ['img', 'bad', 'mask'],
  });
  await mgr.onStopped(1);
  assert.equal(sent.length, 2);
  assert.equal(warnings.length, 0); // auto re-plot never toasts
});

test('plotVariable enrolls the variable so a later stop re-plots it', async () => {
  const { sent, mgr } = harness();
  await mgr.plotVariable('img', 1);
  assert.equal(sent.length, 1);
  await mgr.onStopped(1);
  assert.equal(sent.length, 2);
});

test('plotVariable failure warns and does not enroll', async () => {
  const { sent, warnings, mgr } = harness();
  await mgr.plotVariable('bad', 1);
  assert.equal(sent.length, 0);
  assert.equal(warnings.length, 1);
  await mgr.onStopped(1);
  assert.equal(sent.length, 0);
});

test('onStopped re-plots the union of live set and watch list, deduped', async () => {
  const { sent, mgr } = harness({ getWatchList: () => ['mask', 'img'] });
  await mgr.plotVariable('img', 1);
  sent.length = 0;
  await mgr.onStopped(1);
  assert.equal(sent.length, 2); // {img, mask}, img not duplicated
});

test('a live variable that fails one stop re-plots when it returns to scope', async () => {
  let inScope = true;
  const { sent, mgr } = harness({
    source: {
      async getBufferMetadata(name) {
        if (!inScope) throw new Error('out of scope');
        return params(name);
      },
    },
  });
  await mgr.plotVariable('img', 1); // enrolled
  inScope = false;
  await mgr.onStopped(1); // silent failure, retained
  inScope = true;
  sent.length = 0;
  await mgr.onStopped(1);
  assert.equal(sent.length, 1); // resumed
});

test('reset clears the live set', async () => {
  const { sent, mgr } = harness();
  await mgr.plotVariable('img', 1);
  mgr.reset();
  sent.length = 0;
  await mgr.onStopped(1);
  assert.equal(sent.length, 0);
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && rm -rf out && npm run compile && npm test`
Expected: compile succeeds; the new tests FAIL (e.g. `reset is not a function`, enrolled re-plot counts wrong) and the updated silent test FAILS on `warnings.length` until the implementation changes.

- [ ] **Step 3: Rewrite the SessionManager class**

Replace the `SessionManager` class in `src/session/session-manager.ts` (keep the interfaces above it unchanged) with:

```ts
export class SessionManager {
  private readonly liveNames = new Set<string>();

  constructor(private readonly deps: SessionDeps) {}

  async plotVariable(name: string, threadId: number): Promise<void> {
    const frameId = await this.deps.resolveFrame(threadId);
    const ok = await this.plotAtFrame(name, frameId, { warnOnError: true });
    if (ok) this.liveNames.add(name);
  }

  async onStopped(threadId: number): Promise<void> {
    const names = this.effectiveNames();
    if (names.length === 0) return;
    const frameId = await this.deps.resolveFrame(threadId);
    for (const name of names) {
      await this.plotAtFrame(name, frameId, { warnOnError: false });
    }
  }

  reset(): void {
    this.liveNames.clear();
  }

  private effectiveNames(): string[] {
    const set = new Set<string>(this.liveNames);
    for (const name of this.deps.getWatchList()) set.add(name);
    return [...set];
  }

  private async plotAtFrame(
    name: string,
    frameId: number,
    opts: { warnOnError: boolean }
  ): Promise<boolean> {
    try {
      const p = await this.deps.source.getBufferMetadata(name, frameId);
      await this.deps.sink.ensureReady();
      await this.deps.sink.sendPlotBuffer(this.deps.encode(p));
      return true;
    } catch (err) {
      if (opts.warnOnError) {
        this.deps.warn(
          `OID: could not plot '${name}': ${err instanceof Error ? err.message : String(err)}`
        );
      }
      return false;
    }
  }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile && npm test`
Expected: all `session-manager.test.ts` tests PASS; no other test regresses. (If compile prints errors, fix them — do not trust a stale green from a prior `out/`.)

- [ ] **Step 5: Commit**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode
git add src/session/session-manager.ts test/session-manager.test.ts
git commit -m "feat(session): remember plotted vars and re-plot them on each stop"
```

---

### Task 2: Overlap guard — skip a re-plot while one is in flight

**Files:**
- Modify: `src/session/session-manager.ts` (`SessionManager`)
- Test: `test/session-manager.test.ts` (append one test)

**Interfaces:**
- Consumes: `SessionManager.onStopped` from Task 1.
- Produces: `onStopped` becomes re-entrancy-safe — a call made while a previous `onStopped` is still awaiting is skipped (no second `resolveFrame`).

- [ ] **Step 1: Write the failing test**

Append to `test/session-manager.test.ts`:

```ts
test('onStopped skips a re-plot while one is already in flight', async () => {
  let release!: () => void;
  const gate = new Promise<void>((r) => { release = r; });
  let frameCalls = 0;
  const { mgr } = harness({
    getWatchList: () => ['img'],
    resolveFrame: async () => { frameCalls++; await gate; return 42; },
  });
  const first = mgr.onStopped(1);
  const second = mgr.onStopped(2); // in-flight -> skipped
  release();
  await Promise.all([first, second]);
  assert.equal(frameCalls, 1);
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && rm -rf out && npm run compile && npm test`
Expected: this test FAILS with `frameCalls` equal to `2` (both calls resolve a frame).

- [ ] **Step 3: Add the in-flight guard**

In `src/session/session-manager.ts`, add the field and wrap `onStopped`:

Add the field next to `liveNames`:
```ts
  private replotting = false;
```

Replace `onStopped` with:
```ts
  async onStopped(threadId: number): Promise<void> {
    if (this.replotting) return;
    const names = this.effectiveNames();
    if (names.length === 0) return;
    this.replotting = true;
    try {
      const frameId = await this.deps.resolveFrame(threadId);
      for (const name of names) {
        await this.plotAtFrame(name, frameId, { warnOnError: false });
      }
    } finally {
      this.replotting = false;
    }
  }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile && npm test`
Expected: the new test PASSES (`frameCalls === 1`); all Task 1 tests still PASS.

- [ ] **Step 5: Commit**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode
git add src/session/session-manager.ts test/session-manager.test.ts
git commit -m "feat(session): skip overlapping re-plots on rapid stepping"
```

---

### Task 3: Extension wiring — `oid.clearWatch` command and reset on session end

**Files:**
- Modify: `src/extension.ts`
- Modify: `package.json` (contributes.commands)

**Interfaces:**
- Consumes: `SessionManager.reset()` from Task 1.
- Produces: command `oid.clearWatch` titled "OID: Clear Live Watches"; the live set is cleared on `oid.clearWatch` and on debug-session terminate.

This task is VS Code glue (not unit-testable under `node:test`); its gate is a clean compile plus the existing suite staying green.

- [ ] **Step 1: Reset the live set on session terminate**

In `src/extension.ts`, the existing terminate subscription is:
```ts
  context.subscriptions.push(
    vscode.debug.onDidTerminateDebugSession(() => { stoppedThreadId = undefined; })
  );
```
Replace it with:
```ts
  context.subscriptions.push(
    vscode.debug.onDidTerminateDebugSession(() => {
      stoppedThreadId = undefined;
      manager.reset();
    })
  );
```

- [ ] **Step 2: Register the `oid.clearWatch` command**

In `src/extension.ts`, the existing command registration block is:
```ts
  context.subscriptions.push(
    vscode.commands.registerCommand('oid.openPanel', () => viewer.open()),
    vscode.commands.registerCommand('oid.plot', async () => {
```
Insert a new command registration immediately after the `oid.openPanel` line, so the block begins:
```ts
  context.subscriptions.push(
    vscode.commands.registerCommand('oid.openPanel', () => viewer.open()),
    vscode.commands.registerCommand('oid.clearWatch', () => {
      manager.reset();
      vscode.window.showInformationMessage('OID: live watches cleared.');
    }),
    vscode.commands.registerCommand('oid.plot', async () => {
```

- [ ] **Step 3: Contribute the command in package.json**

In `package.json`, the `contributes.commands` array is:
```json
    "commands": [
      { "command": "oid.openPanel", "title": "OID: Open Viewer Panel" },
      { "command": "oid.plot", "title": "OID: Plot Variable" }
    ],
```
Replace it with:
```json
    "commands": [
      { "command": "oid.openPanel", "title": "OID: Open Viewer Panel" },
      { "command": "oid.plot", "title": "OID: Plot Variable" },
      { "command": "oid.clearWatch", "title": "OID: Clear Live Watches" }
    ],
```

- [ ] **Step 4: Verify it compiles and the suite is green**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && rm -rf out && npm run compile && npm test`
Expected: compile succeeds with no errors; all tests PASS.

- [ ] **Step 5: Commit**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode
git add src/extension.ts package.json
git commit -m "feat(extension): clear-watch command and reset live set on session end"
```

---

## Manual verification (after Task 3)

Run the extension (F5), start a CodeLLDB session, hit a breakpoint, run **OID: Plot Variable** on an image. Continue to the next stop: the image should refresh automatically. Run **OID: Clear Live Watches**; subsequent stops should no longer refresh it. Stopping the debug session and starting a new one should begin with an empty live set.

## Self-review notes
- Spec coverage: live set state + union (Task 1), enroll-on-success / silent-retained failures / reset (Task 1), overlap guard (Task 2), `oid.clearWatch` + terminate reset + config union (Tasks 1 & 3). All spec sections mapped.
- Deferred Option B (viewer-authoritative via `GetObservedSymbols`, needing the C++ `MessageComposer::send` coalescing fix) is intentionally **not** in this plan.
