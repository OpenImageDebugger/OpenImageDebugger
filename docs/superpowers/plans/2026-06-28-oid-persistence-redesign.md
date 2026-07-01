# OID Persistence Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persist OID viewer state across VS Code restarts with behavioral parity to desktop C++ — global UI prefs plus an auto-captured, 1-day-expiring per-workspace set of plotted buffers that auto-replot in scope — without regressing autocompletion or plotting.

**Architecture:** The extension is the single source of truth, split into two independent subsystems. **Buffers** (per-workspace) are owned entirely by the extension and synced via *existing* IPC (`GetObservedSymbols` 0/1 + `PlotBufferContents` 3) — no C++ change, replot decision never depends on IPC ordering. **UI prefs** (global) sync via new IPC `ApplySessionState` (6) and `SessionStateChanged` (7), applied/serialized in the viewer through the existing `SettingsApplier`.

**Tech Stack:** TypeScript (`node:test`), VS Code Extension API, Qt 6 / C++20, `QJsonDocument`, Emscripten WASM, legacy IPC wire format (`wasm32` little-endian u32 length-prefixed strings).

**Spec:** `docs/superpowers/specs/2026-06-28-oid-persistence-redesign-design.md`

## Global Constraints

- **Repos:** OID C++/WASM in `/Users/bruno/ws/OpenImageDebugger` (branch `feat/wasm-vscode`); extension in `/Users/bruno/ws/openimagedebugger-vscode` (branch `feat/live-update-local-wasm`).
- **Wire format:** `wasm32` — message-type header (u32) + u32 length-prefixed UTF-8 strings (no NUL).
- **Message types:** `ApplySessionState = 6`, `SessionStateChanged = 7` already declared in both `message_exchange.h` and `message-exchange.ts`. `float` is already in the C++ `PrimitiveType` concept.
- **Storage scope:** UI prefs → `context.globalState`; buffer set → `context.workspaceState` (already per-workspace).
- **Buffer expiry:** 1 day (`BUFFER_EXPIRATION_DAYS = 1`); persist debounce 100 ms (`SETTINGS_PERSIST_DELAY_MS`).
- **Wire JSON shape** (types 6/7 payload, prefs only — no buffer keys):
  ```json
  {
    "rendering": { "framerate": 60 },
    "export": { "defaultSuffix": "Image File (*.png)" },
    "ui": {
      "splitterSizes": [200, 400],
      "minmaxVisible": true,
      "contrastEnabled": false,
      "linkViewsEnabled": false,
      "listPosition": "left",
      "colorspace": "rgba"
    }
  }
  ```
- **Extension build/test:** `cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile && npm test`.
- **OID native test:** `cd /Users/bruno/ws/OpenImageDebugger && cmake build && cmake --build build --target test_session_state_codec && ctest --test-dir build -R SessionStateCodec`.
- **OID WASM build + media sync:** `cd /Users/bruno/ws/OpenImageDebugger && bash wasm/scripts/build-wasm.sh --reconfigure && bash wasm/scripts/pack.sh`.
- **Pre-existing untracked WIP:** OID may contain untracked `src/ui/messaging/session_state_{codec.h,codec.cpp,apply.cpp}` and `tests/test_session_state_codec.cpp`; the extension may contain untracked `test/message-exchange-session.test.ts`. The C++/test tasks below define the **authoritative** content — overwrite any WIP.
- **No `Co-Authored-By` trailer in commits.**

---

## File map

| File | Repo | Responsibility |
|------|------|----------------|
| `src/session/buffer-store.ts` | ext | **new** — per-workspace `wanted` set: seed, prune, refresh, removal-diff, persist |
| `test/buffer-store.test.ts` | ext | **new** — buffer-store unit tests |
| `src/session/pref-store.ts` | ext | **new** — global prefs: defaults, deep-merge, encode, applyDelta, debounce |
| `test/pref-store.test.ts` | ext | **new** — pref-store unit tests |
| `src/session/session-manager.ts` | ext | wire to buffer-store (replot `wanted ∪ held`, capture held) |
| `test/session-manager.test.ts` | ext | updated for new deps |
| `src/ipc/message-exchange.ts` | ext | codecs: build type 6, decode type 7 |
| `test/message-exchange-session.test.ts` | ext | **new** — IPC 6/7 tests |
| `src/webview/panel.ts` | ext | send type 6 on ready; route type 7 → handler |
| `src/extension.ts` | ext | construct stores; lifecycle wiring |
| `src/session/persistence.ts` + `test/persistence.test.ts` | ext | **removed** |
| `src/ui/messaging/session_state_codec.{h,cpp}` | OID | prefs JSON parse + serialize (prefs only) |
| `src/ui/messaging/session_state_apply.cpp` | OID | apply prefs via `SettingsApplier` |
| `tests/test_session_state_codec.cpp` | OID | native codec unit tests |
| `src/ui/messaging/message_handler.{h,cpp}` | OID | decode type 6 → applier |
| `src/ui/main_window/main_window.cpp` | OID | EMSCRIPTEN persist→type 7; skip QSettings load |
| `src/CMakeLists.txt`, `tests/CMakeLists.txt` | OID | register codec sources + test |

---

### Task 1: Remove dead persistence module (extension)

`src/session/persistence.ts` is committed but unreferenced (verified: no imports in `src/`). It is replaced by `buffer-store.ts` + `pref-store.ts`.

**Files:**
- Delete: `openimagedebugger-vscode/src/session/persistence.ts`
- Delete: `openimagedebugger-vscode/test/persistence.test.ts`

- [ ] **Step 1: Confirm it is unreferenced**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && grep -rn "persistence" src/`
Expected: no matches.

- [ ] **Step 2: Delete the files**

```bash
git rm src/session/persistence.ts test/persistence.test.ts
```

- [ ] **Step 3: Compile and test**

Run: `npm run compile && npm test`
Expected: compiles; remaining tests pass (the persistence tests are gone).

- [ ] **Step 4: Commit**

```bash
git commit -m "refactor(session): remove unused persistence module"
```

---

### Task 2: Buffer-store (extension, per-workspace)

**Files:**
- Create: `openimagedebugger-vscode/src/session/buffer-store.ts`
- Create: `openimagedebugger-vscode/test/buffer-store.test.ts`

**Interfaces:**
- Produces:
  - `BUFFER_EXPIRY_MS: number`
  - `interface WantedBuffer { name: string; expiresAt: number }`
  - `pruneExpired(set, now): WantedBuffer[]`
  - `seedWatchList(set, names, now): WantedBuffer[]`
  - `applyStop(set, held, prevHeld, now): WantedBuffer[]`
  - `class BufferStore { startSession(watchList, now?): void; names(now?): string[]; recordHeld(held, now?): void; reset(): void }`

- [ ] **Step 1: Write the failing tests**

Create `test/buffer-store.test.ts`:

```typescript
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import {
  BUFFER_EXPIRY_MS,
  applyStop,
  pruneExpired,
  seedWatchList,
  BufferStore,
  type WantedBuffer,
} from '../src/session/buffer-store.js';

test('pruneExpired drops entries past their expiry', () => {
  const now = 1_000;
  const out = pruneExpired(
    [
      { name: 'keep', expiresAt: now + 1 },
      { name: 'gone', expiresAt: now - 1 },
    ],
    now
  );
  assert.deepEqual(out.map((b) => b.name), ['keep']);
});

test('seedWatchList adds new names with fresh expiry, skips known/empty', () => {
  const now = 5_000;
  const out = seedWatchList([{ name: 'a', expiresAt: now + 10 }], ['a', '', 'b'], now);
  assert.deepEqual(out.map((b) => b.name).sort(), ['a', 'b']);
  assert.equal(out.find((b) => b.name === 'b')?.expiresAt, now + BUFFER_EXPIRY_MS);
  assert.equal(out.find((b) => b.name === 'a')?.expiresAt, now + 10); // unchanged
});

test('applyStop refreshes held, keeps unexpired out-of-scope, drops expired', () => {
  const now = 2_000;
  const set: WantedBuffer[] = [
    { name: 'held', expiresAt: now + 1 },
    { name: 'offscreen', expiresAt: now + 999 },
    { name: 'stale', expiresAt: now - 1 },
  ];
  const out = applyStop(set, ['held'], [], now);
  const byName = new Map(out.map((b) => [b.name, b.expiresAt]));
  assert.deepEqual([...byName.keys()].sort(), ['held', 'offscreen']);
  assert.equal(byName.get('held'), now + BUFFER_EXPIRY_MS);
  assert.equal(byName.get('offscreen'), now + 999);
});

test('applyStop drops a buffer that left the held set (removal)', () => {
  const now = 3_000;
  const set: WantedBuffer[] = [{ name: 'x', expiresAt: now + 999 }];
  const out = applyStop(set, [], ['x'], now); // x was held last stop, now absent → removed
  assert.deepEqual(out, []);
});

test('BufferStore round-trips through a memento and seeds once', () => {
  const store = new Map<string, unknown>();
  const memento = {
    get: <T>(k: string, d: T): T => (store.has(k) ? (store.get(k) as T) : d),
    update: (k: string, v: unknown) => {
      store.set(k, v);
      return Promise.resolve();
    },
    keys: () => [...store.keys()],
  };
  const now = 10_000;
  const bs = new BufferStore(memento as never);
  bs.startSession(['seed'], now);
  assert.deepEqual(bs.names(now), ['seed']);
  bs.recordHeld(['seed', 'live'], now);
  assert.deepEqual(bs.names(now).sort(), ['live', 'seed']);
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npm run compile && npm test`
Expected: FAIL — `Cannot find module '../src/session/buffer-store.js'`.

- [ ] **Step 3: Implement buffer-store**

Create `src/session/buffer-store.ts`:

```typescript
import type * as vscode from 'vscode';

/** 1 day, matching desktop BUFFER_EXPIRATION_DAYS. */
export const BUFFER_EXPIRY_MS = 24 * 60 * 60 * 1000;
const BUFFERS_KEY = 'oid.session.buffers.v1';

export interface WantedBuffer {
  name: string;
  expiresAt: number; // unix ms
}

export function pruneExpired(set: readonly WantedBuffer[], now: number): WantedBuffer[] {
  return set.filter((b) => b.expiresAt >= now);
}

export function seedWatchList(
  set: readonly WantedBuffer[],
  names: readonly string[],
  now: number
): WantedBuffer[] {
  const result = [...set];
  const known = new Set(result.map((b) => b.name));
  for (const name of names) {
    if (name.length === 0 || known.has(name)) continue;
    result.push({ name, expiresAt: now + BUFFER_EXPIRY_MS });
    known.add(name);
  }
  return result;
}

/**
 * Desktop merge algorithm: drop buffers that left the held set since the last
 * stop (removed) or expired; keep unexpired out-of-scope buffers; refresh every
 * currently-held buffer with a fresh expiry.
 */
export function applyStop(
  set: readonly WantedBuffer[],
  held: readonly string[],
  prevHeld: readonly string[],
  now: number
): WantedBuffer[] {
  const heldSet = new Set(held);
  const removed = new Set(prevHeld.filter((n) => !heldSet.has(n)));
  const map = new Map<string, number>();
  for (const b of set) {
    if (removed.has(b.name)) continue;
    if (b.expiresAt < now) continue;
    map.set(b.name, b.expiresAt);
  }
  for (const name of held) {
    map.set(name, now + BUFFER_EXPIRY_MS);
  }
  return [...map.entries()].map(([name, expiresAt]) => ({ name, expiresAt }));
}

/** Per-workspace auto-replot set; back it with `context.workspaceState`. */
export class BufferStore {
  private wanted: WantedBuffer[] = [];
  private prevHeld: string[] = [];
  private saveTimer: ReturnType<typeof setTimeout> | undefined;

  constructor(private readonly memento: vscode.Memento) {}

  /** Seed once per debug session: load persisted, prune, merge watch list. */
  startSession(watchList: readonly string[], now = Date.now()): void {
    const stored = this.memento.get<WantedBuffer[]>(BUFFERS_KEY, []);
    this.wanted = seedWatchList(pruneExpired(stored, now), watchList, now);
    this.prevHeld = [];
  }

  /** Non-expired names to attempt to replot. */
  names(now = Date.now()): string[] {
    return pruneExpired(this.wanted, now).map((b) => b.name);
  }

  /** Capture currently-held buffers: refresh/drop/keep, then persist. */
  recordHeld(held: readonly string[], now = Date.now()): void {
    this.wanted = applyStop(this.wanted, held, this.prevHeld, now);
    this.prevHeld = [...held];
    this.saveDebounced();
  }

  /** Clear the in-session diff baseline at session end (keeps persisted set). */
  reset(): void {
    this.prevHeld = [];
  }

  private saveDebounced(delayMs = 100): void {
    if (this.saveTimer) clearTimeout(this.saveTimer);
    this.saveTimer = setTimeout(() => {
      this.saveTimer = undefined;
      void this.memento.update(BUFFERS_KEY, this.wanted);
    }, delayMs);
  }
}
```

- [ ] **Step 4: Run tests**

Run: `npm run compile && npm test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/session/buffer-store.ts test/buffer-store.test.ts
git commit -m "feat(session): add per-workspace buffer-store with desktop merge semantics"
```

---

### Task 3: Wire buffer-store into session-manager + extension

**Files:**
- Modify: `openimagedebugger-vscode/src/session/session-manager.ts`
- Modify: `openimagedebugger-vscode/test/session-manager.test.ts`
- Modify: `openimagedebugger-vscode/src/extension.ts`

**Interfaces:**
- Consumes: `BufferStore` from Task 2.
- Produces: `SessionDeps` now has `beginBufferSession: () => void`, `wantedBuffers: () => string[]`, `captureHeld: (held: string[]) => void` (replaces `getWatchList`).

- [ ] **Step 1: Update session-manager deps and `onStopped`**

In `src/session/session-manager.ts`, replace the `getWatchList` field in `SessionDeps`:

```typescript
  // remove: getWatchList: () => string[];
  beginBufferSession: () => void;
  wantedBuffers: () => string[];
  captureHeld: (held: string[]) => void;
```

Replace the body of `onStopped` with:

```typescript
  async onStopped(threadId: number): Promise<void> {
    if (this.replotting) return;
    this.replotting = true;
    try {
      const frameId = await this.deps.resolveFrame(threadId);
      try {
        const syms = await this.deps.listScopeSymbols(frameId);
        await this.deps.channel.setAvailableSymbols(syms.map((s) => s.name));
      } catch (err) {
        this.deps.warn(
          `OID: could not list symbols: ${err instanceof Error ? err.message : String(err)}`
        );
      }
      if (!this.seeded) {
        this.deps.beginBufferSession();
        this.seeded = true;
      }
      const observed = await this.deps.channel.getObservedSymbols();
      const targets = [...new Set([...observed, ...this.deps.wantedBuffers()])];
      const succeeded: string[] = [];
      for (const name of targets) {
        if (await this.plotAtFrame(name, frameId, { warnOnError: true })) {
          succeeded.push(name);
        }
      }
      const held = [...new Set([...observed, ...succeeded])];
      this.deps.captureHeld(held);
    } catch {
      // Auto-replot is best-effort and silent when frame resolution fails.
    } finally {
      this.replotting = false;
    }
  }
```

`reset()` stays as-is (`this.seeded = false`).

- [ ] **Step 2: Update the session-manager tests for the new deps**

In `test/session-manager.test.ts`, edit the `harness` to drive a fake buffer-store. Replace the `getWatchList: () => [],` line in `deps` with:

```typescript
    beginBufferSession: () => {
      wanted = [...new Set([...wanted, ...seedNames])];
    },
    wantedBuffers: () => wanted,
    captureHeld: (held) => {
      wanted = [...new Set([...wanted, ...held])];
    },
```

Add these locals near the top of `harness` (beside `let observed`):

```typescript
  let wanted: string[] = [];
  let seedNames: string[] = [];
```

Add a setter to the returned object (beside `setObserved`):

```typescript
    setSeed(names: string[]) {
      seedNames = names;
    },
```

Replace the two watch-list tests with buffer-store equivalents:

```typescript
test('onStopped seeds the buffer set on first stop only', async () => {
  const h = harness();
  h.setSeed(['seed']);
  h.setObserved(['live']);
  await h.mgr.onStopped(1);
  assert.equal(h.sent.length, 2); // live + seed

  h.setObserved(['live']);
  h.sent.length = 0;
  await h.mgr.onStopped(1);
  assert.equal(h.sent.length, 2); // live (observed) + seed (now in wanted), seeded not re-run
});

test('reset clears seeded so the buffer session is begun again', async () => {
  const h = harness();
  h.setSeed(['seed']);
  h.setObserved([]);
  await h.mgr.onStopped(1);
  assert.equal(h.sent.length, 1); // seed

  h.mgr.reset();
  h.setObserved([]);
  h.sent.length = 0;
  await h.mgr.onStopped(1);
  assert.equal(h.sent.length, 1); // seed again
});
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `npm run compile && npm test`
Expected: FAIL — `extension.ts` still passes `getWatchList` (compile error) and/or session-manager tests fail until extension wiring is updated. Proceed to Step 4 before re-running.

- [ ] **Step 4: Wire the store in `extension.ts`**

In `src/extension.ts` add the import:

```typescript
import { BufferStore } from './session/buffer-store.js';
```

After `const viewer = new ViewerController(context);` add:

```typescript
  const bufferStore = new BufferStore(context.workspaceState);
```

In the `new SessionManager({ ... })` deps, replace `getWatchList: watchList,` with:

```typescript
    beginBufferSession: () => bufferStore.startSession(watchList()),
    wantedBuffers: () => bufferStore.names(),
    captureHeld: (held) => bufferStore.recordHeld(held),
```

In the `onDidTerminateDebugSession` handler, add `bufferStore.reset();` next to `manager.reset();`.

- [ ] **Step 5: Run tests**

Run: `npm run compile && npm test`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/session/session-manager.ts test/session-manager.test.ts src/extension.ts
git commit -m "feat(session): drive auto-replot from the persisted buffer set"
```

---

### Task 4: Pref-store (extension, global)

**Files:**
- Create: `openimagedebugger-vscode/src/session/pref-store.ts`
- Create: `openimagedebugger-vscode/test/pref-store.test.ts`

**Interfaces:**
- Produces:
  - `interface OidPrefs { rendering:{framerate:number}; export:{defaultSuffix:string}; ui:{ splitterSizes:number[]; minmaxVisible:boolean; contrastEnabled:boolean; linkViewsEnabled:boolean; listPosition?:string; colorspace?:string } }`
  - `type PrefsDelta` (deep-partial of `OidPrefs`)
  - `defaultPrefs(): OidPrefs`
  - `mergePrefs(base, partial): OidPrefs`
  - `class PrefStore { get(): OidPrefs; encode(): string; applyDelta(json): void }`

- [ ] **Step 1: Write the failing tests**

Create `test/pref-store.test.ts`:

```typescript
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import {
  defaultPrefs,
  mergePrefs,
  PrefStore,
} from '../src/session/pref-store.js';

test('defaultPrefs matches desktop SettingsConstants', () => {
  const d = defaultPrefs();
  assert.equal(d.rendering.framerate, 60);
  assert.equal(d.export.defaultSuffix, 'Image File (*.png)');
  assert.equal(d.ui.minmaxVisible, true);
  assert.equal(d.ui.contrastEnabled, false);
});

test('mergePrefs deep-merges nested groups', () => {
  const out = mergePrefs(defaultPrefs(), {
    ui: { contrastEnabled: true, colorspace: 'bgra' },
    rendering: { framerate: 30 },
  });
  assert.equal(out.ui.contrastEnabled, true);
  assert.equal(out.ui.colorspace, 'bgra');
  assert.equal(out.rendering.framerate, 30);
  assert.equal(out.export.defaultSuffix, 'Image File (*.png)'); // untouched
});

test('PrefStore.applyDelta merges and encode round-trips', () => {
  const store = new Map<string, unknown>();
  const memento = {
    get: <T>(k: string, dft: T): T => (store.has(k) ? (store.get(k) as T) : dft),
    update: (k: string, v: unknown) => {
      store.set(k, v);
      return Promise.resolve();
    },
    keys: () => [...store.keys()],
  };
  const ps = new PrefStore(memento as never);
  ps.applyDelta(JSON.stringify({ ui: { linkViewsEnabled: true } }));
  assert.equal(ps.get().ui.linkViewsEnabled, true);
  assert.equal(JSON.parse(ps.encode()).ui.linkViewsEnabled, true);
});

test('PrefStore.applyDelta ignores invalid JSON', () => {
  const memento = { get: <T>(_k: string, d: T) => d, update: () => Promise.resolve(), keys: () => [] };
  const ps = new PrefStore(memento as never);
  ps.applyDelta('{not json');
  assert.equal(ps.get().rendering.framerate, 60); // unchanged
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npm run compile && npm test`
Expected: FAIL — `Cannot find module '../src/session/pref-store.js'`.

- [ ] **Step 3: Implement pref-store**

Create `src/session/pref-store.ts`:

```typescript
import type * as vscode from 'vscode';

const PREFS_KEY = 'oid.session.prefs.v1';

export interface OidPrefs {
  rendering: { framerate: number };
  export: { defaultSuffix: string };
  ui: {
    splitterSizes: number[];
    minmaxVisible: boolean;
    contrastEnabled: boolean;
    linkViewsEnabled: boolean;
    listPosition?: string;
    colorspace?: string;
  };
}

export type PrefsDelta = {
  rendering?: Partial<OidPrefs['rendering']>;
  export?: Partial<OidPrefs['export']>;
  ui?: Partial<OidPrefs['ui']>;
};

export function defaultPrefs(): OidPrefs {
  return {
    rendering: { framerate: 60 },
    export: { defaultSuffix: 'Image File (*.png)' },
    ui: {
      splitterSizes: [],
      minmaxVisible: true,
      contrastEnabled: false,
      linkViewsEnabled: false,
    },
  };
}

export function mergePrefs(base: OidPrefs, partial: PrefsDelta): OidPrefs {
  return {
    rendering: { ...base.rendering, ...partial.rendering },
    export: { ...base.export, ...partial.export },
    ui: { ...base.ui, ...partial.ui },
  };
}

/** Global UI prefs; back it with `context.globalState`. */
export class PrefStore {
  private prefs: OidPrefs;
  private saveTimer: ReturnType<typeof setTimeout> | undefined;

  constructor(private readonly memento: vscode.Memento) {
    this.prefs = mergePrefs(defaultPrefs(), this.memento.get<PrefsDelta>(PREFS_KEY, {}));
  }

  get(): OidPrefs {
    return this.prefs;
  }

  /** Full snapshot to send as ApplySessionState (type 6). */
  encode(): string {
    return JSON.stringify(this.prefs);
  }

  /** Merge a SessionStateChanged (type 7) partial; persist debounced. */
  applyDelta(json: string): void {
    let partial: PrefsDelta;
    try {
      partial = JSON.parse(json) as PrefsDelta;
    } catch (err) {
      console.warn('OID: invalid SessionStateChanged JSON', err);
      return;
    }
    this.prefs = mergePrefs(this.prefs, partial);
    this.saveDebounced();
  }

  private saveDebounced(delayMs = 100): void {
    if (this.saveTimer) clearTimeout(this.saveTimer);
    this.saveTimer = setTimeout(() => {
      this.saveTimer = undefined;
      void this.memento.update(PREFS_KEY, this.prefs);
    }, delayMs);
  }
}
```

- [ ] **Step 4: Run tests**

Run: `npm run compile && npm test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/session/pref-store.ts test/pref-store.test.ts
git commit -m "feat(session): add global pref-store"
```

---

### Task 5: IPC codecs for types 6/7 (extension)

**Files:**
- Modify: `openimagedebugger-vscode/src/ipc/message-exchange.ts`
- Create: `openimagedebugger-vscode/test/message-exchange-session.test.ts`

**Interfaces:**
- Produces: `buildApplySessionState(json, mode?): Uint8Array`; `InboundMessage` gains `{ kind: 'sessionStateChanged'; json: string }`.

- [ ] **Step 1: Write the failing tests**

Create `test/message-exchange-session.test.ts`:

```typescript
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import {
  MessageType,
  buildApplySessionState,
  decodeInbound,
} from '../src/ipc/message-exchange.js';

test('buildApplySessionState encodes type 6 + JSON string', () => {
  const json = '{"rendering":{"framerate":60}}';
  const bytes = buildApplySessionState(json);
  assert.equal(bytes[0], MessageType.ApplySessionState);
  const len = bytes[4] | (bytes[5] << 8) | (bytes[6] << 16) | (bytes[7] << 24);
  assert.equal(len, new TextEncoder().encode(json).length);
  assert.equal(new TextDecoder().decode(bytes.subarray(8)), json);
});

test('decodeInbound parses SessionStateChanged JSON', () => {
  const json = '{"ui":{"contrastEnabled":true}}';
  const enc = new TextEncoder().encode(json);
  const parts = new Uint8Array(4 + 4 + enc.length);
  new DataView(parts.buffer).setUint32(0, MessageType.SessionStateChanged, true);
  new DataView(parts.buffer).setUint32(4, enc.length, true);
  parts.set(enc, 8);
  const decoded = decodeInbound(parts);
  assert.equal(decoded.kind, 'sessionStateChanged');
  if (decoded.kind === 'sessionStateChanged') {
    assert.equal(JSON.parse(decoded.json).ui.contrastEnabled, true);
  }
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npm run compile && npm test`
Expected: FAIL — `buildApplySessionState is not exported`.

- [ ] **Step 3: Implement the codecs**

In `src/ipc/message-exchange.ts`, after `buildExportSelectedBuffer`, add:

```typescript
/** Encode an ApplySessionState (type 6) message carrying a prefs JSON snapshot. */
export function buildApplySessionState(
  json: string,
  mode: WasmLengthMode = 'wasm32'
): Uint8Array {
  const parts: number[] = [];
  pushU32(parts, MessageType.ApplySessionState);
  pushString(parts, json, mode);
  return new Uint8Array(parts);
}
```

In the `InboundMessage` union, add before `| { kind: 'unknown'; ... }`:

```typescript
  | { kind: 'sessionStateChanged'; json: string }
```

In the `decodeInbound` `switch`, add before `default:`:

```typescript
    case MessageType.SessionStateChanged:
      return { kind: 'sessionStateChanged', json: readString() };
```

- [ ] **Step 4: Run tests**

Run: `npm run compile && npm test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/ipc/message-exchange.ts test/message-exchange-session.test.ts
git commit -m "feat(ipc): add ApplySessionState/SessionStateChanged codecs"
```

---

### Task 6: Pref lifecycle wiring (extension)

**Files:**
- Modify: `openimagedebugger-vscode/src/webview/panel.ts`
- Modify: `openimagedebugger-vscode/src/extension.ts`

**Interfaces:**
- Consumes: `buildApplySessionState` (Task 5), `PrefStore` (Task 4).
- Produces: `ViewerController.setSessionStateChangedHandler(fn)`, `ViewerController.queueSessionRestore(json)`.

- [ ] **Step 1: Add session hooks to `ViewerController`**

In `src/webview/panel.ts`, add `buildApplySessionState` to the import from `../ipc/message-exchange.js`.

Add fields after `onExportBufferRequest`:

```typescript
  private onSessionStateChanged: ((json: string) => void) | undefined;
  private sessionRestoreJson: string | undefined;
  private restoreSent = false;
```

Add methods next to `setExportBufferRequestHandler`:

```typescript
  setSessionStateChangedHandler(handler: (json: string) => void): void {
    this.onSessionStateChanged = handler;
  }

  /** Queue a prefs snapshot; sent now if ready, else on viewer-ready. */
  queueSessionRestore(json: string): void {
    this.sessionRestoreJson = json;
    this.restoreSent = false;
    void this.maybeSendSessionRestore();
  }

  private async maybeSendSessionRestore(): Promise<void> {
    if (!this.ready || this.restoreSent || this.sessionRestoreJson === undefined) return;
    this.restoreSent = true;
    await this.sendFrame(buildApplySessionState(this.sessionRestoreJson));
  }
```

In `onDidReceiveMessage`, in the `viewer-ready` branch, after `for (const w of this.readyWaiters.splice(0)) w();` add:

```typescript
        void this.maybeSendSessionRestore();
```

In the `oid-ipc-out` decode chain, after the `exportRequest` branch add:

```typescript
        } else if (decoded.kind === 'sessionStateChanged') {
          this.onSessionStateChanged?.(decoded.json);
```

In both `panel.onDidDispose(...)` and `dispose()`, add `this.restoreSent = false;` next to `this.ready = false;`.

- [ ] **Step 2: Wire `extension.ts`**

In `src/extension.ts` add the import:

```typescript
import { PrefStore } from './session/pref-store.js';
```

After `const bufferStore = new BufferStore(context.workspaceState);` add:

```typescript
  const prefStore = new PrefStore(context.globalState);

  viewer.setSessionStateChangedHandler((json) => prefStore.applyDelta(json));

  const restorePrefs = (): void => {
    viewer.queueSessionRestore(prefStore.encode());
  };
```

In the `stopped` handler's `if (!openedThisSession) { ... }` block, after `viewer.openBeside();` add `restorePrefs();`.

In the `oid.openPanel` command, change the body to:

```typescript
    vscode.commands.registerCommand('oid.openPanel', () => {
      viewer.openBeside();
      restorePrefs();
    }),
```

In the export handler (`viewer.setExportBufferRequestHandler`), change the final `'Image File (*.png)'` argument to:

```typescript
      prefStore.get().export.defaultSuffix
```

- [ ] **Step 3: Compile and test**

Run: `npm run compile && npm test`
Expected: compiles; all tests pass.

- [ ] **Step 4: Commit**

```bash
git add src/webview/panel.ts src/extension.ts
git commit -m "feat(session): restore and persist UI prefs over IPC"
```

---

### Task 7: C++ session_state_codec (prefs only)

**Files:**
- Create: `OpenImageDebugger/src/ui/messaging/session_state_codec.h`
- Create: `OpenImageDebugger/src/ui/messaging/session_state_codec.cpp`
- Create: `OpenImageDebugger/src/ui/messaging/session_state_apply.cpp`
- Create: `OpenImageDebugger/tests/test_session_state_codec.cpp`
- Modify: `OpenImageDebugger/src/CMakeLists.txt`
- Modify: `OpenImageDebugger/tests/CMakeLists.txt`

**Interfaces:**
- Produces: `parse_session_state_json(QByteArray, SessionStateFields&) -> bool`; `serialize_session_state_delta(DataCallbacks, SessionStateExtraInputs) -> QByteArray`; `apply_session_state_fields(SessionStateFields, SettingsApplier&)`.

(All files use the standard OID MIT license header — copy it from any existing file in `src/ui/messaging/`.)

- [ ] **Step 1: Write the failing native test**

Create `tests/test_session_state_codec.cpp` (with license header):

```cpp
#include <QCoreApplication>
#include <gtest/gtest.h>

#include "ui/messaging/session_state_codec.h"

using namespace oid;

namespace {
QCoreApplication& GetApplication() {
    static int argc = 1;
    static std::string app_name = "test";
    static char* argv[] = {app_name.data(), nullptr};
    static QCoreApplication application(argc, argv);
    return application;
}
} // namespace

TEST(SessionStateCodec, ParsesPrefs) {
    GetApplication();
    const auto json = QByteArray(
        R"({"rendering":{"framerate":30},"ui":{"contrastEnabled":true,)"
        R"("listPosition":"bottom","colorspace":"bgra"}})");
    SessionStateFields fields;
    ASSERT_TRUE(parse_session_state_json(json, fields));
    ASSERT_TRUE(fields.framerate.has_value());
    EXPECT_DOUBLE_EQ(*fields.framerate, 30.0);
    ASSERT_TRUE(fields.contrast_enabled.has_value());
    EXPECT_TRUE(*fields.contrast_enabled);
    ASSERT_TRUE(fields.list_position.has_value());
    EXPECT_EQ(*fields.list_position, QStringLiteral("bottom"));
    ASSERT_TRUE(fields.colorspace.has_value());
    EXPECT_EQ(*fields.colorspace, QStringLiteral("bgra"));
}

TEST(SessionStateCodec, SerializeIsPrefsOnly) {
    GetApplication();
    SettingsManager::DataCallbacks callbacks;
    callbacks.getRenderFramerate = [] { return 60.0; };
    callbacks.getDefaultExportSuffix = [] { return QStringLiteral("Image File (*.png)"); };
    callbacks.getSplitterSizes = [] { return QList<int>{100, 200}; };
    callbacks.getMinMaxVisible = [] { return true; };
    callbacks.getContrastEnabled = [] { return false; };
    callbacks.getLinkViewsEnabled = [] { return false; };

    SessionStateExtraInputs extra;
    extra.getColorspace = [] { return QStringLiteral("rgba"); };
    extra.getListPosition = [] { return QStringLiteral("left"); };

    const auto json = serialize_session_state_delta(callbacks, extra);
    EXPECT_TRUE(json.contains("\"framerate\""));
    EXPECT_TRUE(json.contains("\"colorspace\""));
    EXPECT_FALSE(json.contains("\"held\""));     // no buffer keys
    EXPECT_FALSE(json.contains("\"buffers\""));
}
```

- [ ] **Step 2: Create the header**

Create `src/ui/messaging/session_state_codec.h` (with license header):

```cpp
#ifndef SESSION_STATE_CODEC_H_
#define SESSION_STATE_CODEC_H_

#include <functional>
#include <optional>

#include <QByteArray>
#include <QList>
#include <QString>

#include "ui/main_window/settings_manager.h"

namespace oid {

class SettingsApplier;

struct SessionStateFields {
    std::optional<double> framerate;
    std::optional<QString> default_export_suffix;
    std::optional<QList<int>> splitter_sizes;
    std::optional<QString> list_position;
    std::optional<bool> minmax_visible;
    std::optional<bool> minmax_compact;
    std::optional<bool> contrast_enabled;
    std::optional<bool> link_views_enabled;
    std::optional<QString> colorspace;
};

struct SessionStateExtraInputs {
    std::function<QString()> getListPosition;
    std::function<QString()> getColorspace;
};

bool parse_session_state_json(const QByteArray& json, SessionStateFields& out);

void apply_session_state_fields(const SessionStateFields& fields,
                                SettingsApplier& applier);

QByteArray serialize_session_state_delta(
    const SettingsManager::DataCallbacks& callbacks,
    const SessionStateExtraInputs& extra);

} // namespace oid

#endif // SESSION_STATE_CODEC_H_
```

- [ ] **Step 3: Implement parse + serialize**

Create `src/ui/messaging/session_state_codec.cpp` (with license header):

```cpp
#include "session_state_codec.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace oid {

namespace {

std::optional<bool> read_bool(const QJsonObject& obj, const char* key) {
    if (!obj.contains(key)) return std::nullopt;
    return obj.value(key).toBool();
}

std::optional<double> read_double(const QJsonObject& obj, const char* key) {
    if (!obj.contains(key)) return std::nullopt;
    return obj.value(key).toDouble();
}

std::optional<QString> read_string(const QJsonObject& obj, const char* key) {
    if (!obj.contains(key)) return std::nullopt;
    return obj.value(key).toString();
}

} // namespace

bool parse_session_state_json(const QByteArray& json, SessionStateFields& out) {
    const auto doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) return false;
    const auto root = doc.object();

    if (const auto rendering = root.value(QStringLiteral("rendering")).toObject();
        !rendering.isEmpty()) {
        out.framerate = read_double(rendering, "framerate");
    }
    if (const auto exp = root.value(QStringLiteral("export")).toObject();
        !exp.isEmpty()) {
        out.default_export_suffix = read_string(exp, "defaultSuffix");
    }
    if (const auto ui = root.value(QStringLiteral("ui")).toObject(); !ui.isEmpty()) {
        out.list_position = read_string(ui, "listPosition");
        out.minmax_visible = read_bool(ui, "minmaxVisible");
        out.minmax_compact = read_bool(ui, "minmaxCompact");
        out.contrast_enabled = read_bool(ui, "contrastEnabled");
        out.link_views_enabled = read_bool(ui, "linkViewsEnabled");
        out.colorspace = read_string(ui, "colorspace");
        if (ui.contains(QStringLiteral("splitterSizes"))) {
            const auto arr = ui.value(QStringLiteral("splitterSizes")).toArray();
            auto sizes = QList<int>{};
            for (const auto& v : arr) sizes.append(v.toInt());
            out.splitter_sizes = sizes;
        }
    }
    return true;
}

QByteArray serialize_session_state_delta(
    const SettingsManager::DataCallbacks& callbacks,
    const SessionStateExtraInputs& extra) {
    auto rendering = QJsonObject{};
    rendering.insert(QStringLiteral("framerate"), callbacks.getRenderFramerate());

    auto exp = QJsonObject{};
    exp.insert(QStringLiteral("defaultSuffix"), callbacks.getDefaultExportSuffix());

    auto ui = QJsonObject{};
    auto sizes = QJsonArray{};
    for (const int s : callbacks.getSplitterSizes()) sizes.append(s);
    ui.insert(QStringLiteral("splitterSizes"), sizes);
    ui.insert(QStringLiteral("minmaxVisible"), callbacks.getMinMaxVisible());
    ui.insert(QStringLiteral("contrastEnabled"), callbacks.getContrastEnabled());
    ui.insert(QStringLiteral("linkViewsEnabled"), callbacks.getLinkViewsEnabled());
    if (extra.getListPosition) {
        if (const auto p = extra.getListPosition(); !p.isEmpty()) {
            ui.insert(QStringLiteral("listPosition"), p);
        }
    }
    if (extra.getColorspace) {
        if (const auto c = extra.getColorspace(); !c.isEmpty()) {
            ui.insert(QStringLiteral("colorspace"), c);
        }
    }

    auto root = QJsonObject{};
    root.insert(QStringLiteral("rendering"), rendering);
    root.insert(QStringLiteral("export"), exp);
    root.insert(QStringLiteral("ui"), ui);
    return QJsonDocument{root}.toJson(QJsonDocument::Compact);
}

} // namespace oid
```

- [ ] **Step 4: Implement apply (separate TU so the test need not link SettingsApplier)**

Create `src/ui/messaging/session_state_apply.cpp` (with license header):

```cpp
#include "session_state_codec.h"

#include "ui/main_window/settings_applier.h"

namespace oid {

namespace {

QString colorspace_channel_from_char(const QChar& character) {
    switch (character.toLatin1()) {
    case 'b': return QStringLiteral("blue");
    case 'g': return QStringLiteral("green");
    case 'r': return QStringLiteral("red");
    case 'a': return QStringLiteral("alpha");
    default: return {};
    }
}

} // namespace

void apply_session_state_fields(const SessionStateFields& fields,
                                SettingsApplier& applier) {
    if (fields.framerate.has_value()) {
        applier.apply_rendering_settings(*fields.framerate);
    }
    if (fields.default_export_suffix.has_value()) {
        applier.apply_export_settings(*fields.default_export_suffix);
    }
    if (fields.list_position.has_value()) {
        applier.apply_ui_list_position(*fields.list_position);
    }
    if (fields.splitter_sizes.has_value()) {
        applier.apply_ui_splitter_sizes(*fields.splitter_sizes);
    }
    if (fields.minmax_compact.has_value() && fields.minmax_visible.has_value()) {
        applier.apply_ui_minmax_compact(*fields.minmax_compact,
                                        *fields.minmax_visible);
    } else if (fields.minmax_visible.has_value()) {
        applier.apply_ui_minmax_visible(*fields.minmax_visible);
    }
    if (fields.colorspace.has_value()) {
        const auto& cs = *fields.colorspace;
        QString ch1, ch2, ch3, ch4;
        if (cs.size() > 0) ch1 = colorspace_channel_from_char(cs.at(0));
        if (cs.size() > 1) ch2 = colorspace_channel_from_char(cs.at(1));
        if (cs.size() > 2) ch3 = colorspace_channel_from_char(cs.at(2));
        if (cs.size() > 3) ch4 = colorspace_channel_from_char(cs.at(3));
        applier.apply_ui_colorspace(ch1, ch2, ch3, ch4);
    }
    if (fields.contrast_enabled.has_value()) {
        applier.apply_ui_contrast_enabled(*fields.contrast_enabled);
    }
    if (fields.link_views_enabled.has_value()) {
        applier.apply_ui_link_views_enabled(*fields.link_views_enabled);
    }
}

} // namespace oid
```

- [ ] **Step 5: Register in CMake**

In `src/CMakeLists.txt`, in the SOURCES list right after `ui/messaging/message_handler.cpp`, add:

```cmake
    ui/messaging/session_state_apply.cpp
    ui/messaging/session_state_codec.cpp
```

In `tests/CMakeLists.txt`, after the `TransportTests` block, add:

```cmake
# Test for session state JSON codec
add_executable(test_session_state_codec
    test_session_state_codec.cpp
)

target_include_directories(test_session_state_codec
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(test_session_state_codec
    PRIVATE
    Qt6::Core
    GTest::gtest_main
    GTest::gtest
)

target_sources(test_session_state_codec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/messaging/session_state_codec.cpp
)

add_test(NAME SessionStateCodec COMMAND test_session_state_codec)
```

- [ ] **Step 6: Build and run the test**

Run: `cmake build && cmake --build build --target test_session_state_codec && ctest --test-dir build -R SessionStateCodec --output-on-failure`
Expected: PASS (2 tests).

- [ ] **Step 7: Verify apply compiles in the main target**

Run: `ninja -C build src/CMakeFiles/oidwindow.dir/ui/messaging/session_state_apply.cpp.o`
Expected: builds with no errors.

- [ ] **Step 8: Commit**

```bash
cd /Users/bruno/ws/OpenImageDebugger
git add src/ui/messaging/session_state_codec.h src/ui/messaging/session_state_codec.cpp \
  src/ui/messaging/session_state_apply.cpp tests/test_session_state_codec.cpp \
  src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(session): add prefs-only session_state_codec"
```

---

### Task 8: C++ MessageHandler decode type 6 + main_window wiring

**Files:**
- Modify: `OpenImageDebugger/src/ui/messaging/message_handler.h`
- Modify: `OpenImageDebugger/src/ui/messaging/message_handler.cpp`
- Modify: `OpenImageDebugger/src/ui/main_window/main_window.cpp`

**Interfaces:**
- Consumes: `parse_session_state_json`, `apply_session_state_fields` (Task 7), `SettingsApplier`.
- Produces: `MessageHandler::Dependencies.settings_applier` (non-owning, may be null).

- [ ] **Step 1: Extend the handler header**

In `src/ui/messaging/message_handler.h`:
- After `struct WindowState;` add `class SettingsApplier;`.
- In the `Dependencies` struct, after the `select_stage` field, add:

```cpp
        // Non-owning; applies inbound ApplySessionState. May be null where no
        // session state is ever received (e.g. desktop).
        SettingsApplier* settings_applier = nullptr;
```

- In the private methods, after `void decode_plot_buffer_contents();` add:

```cpp
    void decode_apply_session_state() const;
```

- [ ] **Step 2: Implement the decode in the handler**

In `src/ui/messaging/message_handler.cpp`, add includes after `#include "ui/main_window/main_window.h"`:

```cpp
#include "ui/main_window/settings_applier.h"
#include "ui/messaging/session_state_codec.h"
```

Add the method before `void MessageHandler::decode_incoming_messages()`:

```cpp
void MessageHandler::decode_apply_session_state() const {
    auto message_decoder = MessageDecoder{deps_.transport};
    auto json = std::string{};
    message_decoder.read(json);

    if (deps_.settings_applier == nullptr) {
        return;
    }
    auto fields = SessionStateFields{};
    if (!parse_session_state_json(QByteArray::fromStdString(json), fields)) {
        std::cerr << "[OID] Invalid ApplySessionState JSON" << std::endl;
        return;
    }
    apply_session_state_fields(fields, *deps_.settings_applier);
}
```

In the `decode_incoming_messages()` switch, before `default:` add:

```cpp
    case MessageType::ApplySessionState:
        decode_apply_session_state();
        break;
```

- [ ] **Step 3: Construct the applier before the handler in `main_window.cpp`**

In `src/ui/main_window/main_window.cpp`, move the `settings_applier_` construction so it precedes the message-handler construction. Insert this block immediately before the `// Initialize message handler` comment:

```cpp
    // Initialize settings applier (before the message handler so inbound
    // ApplySessionState can be applied through it)
    settings_applier_ = std::make_unique<SettingsApplier>(
        SettingsApplier::Dependencies{ui_mutex_,
                                      state_,
                                      ui_components_,
                                      buffer_data_,
                                      channel_names_,
                                      render_framerate_,
                                      default_export_suffix_,
                                      *this},
        this);

```

Then delete the original `// Initialize settings applier` block (the duplicate `settings_applier_ = std::make_unique<SettingsApplier>(...)` that sits just before `connect_settings_signals();`).

- [ ] **Step 4: Pass the applier into both handler dependency lists**

In `main_window.cpp`, in **both** the `#ifdef __EMSCRIPTEN__` and `#else` `MessageHandler::Dependencies{ ... }` initializers, change the trailing `set_currently_selected_stage(stage); }}` to add the applier:

```cpp
            [this](const std::shared_ptr<Stage>& stage) {
                set_currently_selected_stage(stage);
            },
            settings_applier_.get()},
        this);
```

- [ ] **Step 5: Skip the QSettings load under EMSCRIPTEN**

In `main_window.cpp`, replace:

```cpp
    // Load settings (must be done before initialization)
    settings_manager_->load_settings();
```

with:

```cpp
    // Load settings (must be done before initialization).
    // On WASM the extension owns persistence and prefs arrive via
    // ApplySessionState (IPC type 6); skip the local QSettings load.
#ifndef __EMSCRIPTEN__
    settings_manager_->load_settings();
#endif
```

- [ ] **Step 6: Build the affected objects (desktop path)**

Run:
```bash
ninja -C build src/CMakeFiles/oidwindow.dir/ui/messaging/message_handler.cpp.o \
  src/CMakeFiles/oidwindow.dir/ui/main_window/main_window.cpp.o
```
Expected: builds with no errors.

- [ ] **Step 7: Commit**

```bash
git add src/ui/messaging/message_handler.h src/ui/messaging/message_handler.cpp \
  src/ui/main_window/main_window.cpp
git commit -m "feat(session): decode ApplySessionState in MessageHandler"
```

---

### Task 9: C++ WASM persist → SessionStateChanged (prefs only)

**Files:**
- Modify: `OpenImageDebugger/src/ui/main_window/main_window.cpp`

**Interfaces:**
- Consumes: `serialize_session_state_delta`, `SessionStateExtraInputs` (Task 7); `MessageComposer`/`MessageType` (`ipc/message_exchange.h`).

- [ ] **Step 1: Add includes**

In `src/ui/main_window/main_window.cpp`, add near the other local includes:

```cpp
#include "ipc/message_exchange.h"
#include "ui/messaging/session_state_codec.h"
```

- [ ] **Step 2: Emit prefs as type 7 on WASM persist**

In `main_window.cpp`, in `MainWindow::persist_settings()`, replace the final line `SettingsManager::persist_settings(callbacks);` with:

```cpp
#ifdef __EMSCRIPTEN__
    // The VS Code extension owns persistence on WASM: serialize prefs only and
    // stream them as SessionStateChanged (IPC type 7). Buffers are handled
    // extension-side via GetObservedSymbols, not here.
    SessionStateExtraInputs extra;
    extra.getColorspace = [this] {
        const auto to_char = [](const QString& name) -> QChar {
            if (name == QStringLiteral("red")) return QLatin1Char('r');
            if (name == QStringLiteral("green")) return QLatin1Char('g');
            if (name == QStringLiteral("blue")) return QLatin1Char('b');
            if (name == QStringLiteral("alpha")) return QLatin1Char('a');
            return {};
        };
        auto colorspace = QString{};
        for (const auto& name : {channel_names_.name_channel_1,
                                 channel_names_.name_channel_2,
                                 channel_names_.name_channel_3,
                                 channel_names_.name_channel_4}) {
            if (const auto character = to_char(name); !character.isNull()) {
                colorspace.append(character);
            }
        }
        return colorspace;
    };
    extra.getListPosition = [this] {
        auto* const splitter = ui_components_.ui->splitter;
        const auto vertical = splitter->orientation() == Qt::Vertical;
        const auto list_last =
            splitter->indexOf(ui_components_.ui->frame_list) != 0;
        if (vertical) {
            return QString(list_last ? QStringLiteral("bottom")
                                     : QStringLiteral("top"));
        }
        return QString(list_last ? QStringLiteral("right")
                                 : QStringLiteral("left"));
    };

    const auto json = serialize_session_state_delta(callbacks, extra);
    if (postmessage_transport_ != nullptr) {
        auto composer = MessageComposer{};
        composer.push(MessageType::SessionStateChanged)
            .push(std::string(json.constData(),
                              static_cast<std::size_t>(json.size())))
            .send(*postmessage_transport_);
    }
#else
    SettingsManager::persist_settings(callbacks);
#endif
```

(Note: `minmaxCompact` is intentionally not emitted — no readable runtime state; the codec applies `minmaxVisible` alone when it is absent.)

- [ ] **Step 3: Build the object (desktop path verifies includes + non-EMSCRIPTEN branch)**

Run: `ninja -C build src/CMakeFiles/oidwindow.dir/ui/main_window/main_window.cpp.o`
Expected: builds with no errors.

- [ ] **Step 4: Commit**

```bash
git add src/ui/main_window/main_window.cpp
git commit -m "feat(session): emit prefs as SessionStateChanged from WASM persist"
```

---

### Task 10: Build WASM and sync to the extension

**Files:**
- Modify (binary): `openimagedebugger-vscode/media/oidwindow.{wasm,js}`, `media/version.json`

- [ ] **Step 1: Build the WASM viewer (exercises both EMSCRIPTEN paths)**

Run: `cd /Users/bruno/ws/OpenImageDebugger && bash wasm/scripts/build-wasm.sh --reconfigure`
Expected: exits 0; `build-wasm/src/oidwindow.{wasm,js}` produced. (`--reconfigure` is required because new source files were added to `src/CMakeLists.txt`.)

- [ ] **Step 2: Pack and sync to the extension media folder**

Run: `bash wasm/scripts/pack.sh`
Expected: copies artifacts into `openimagedebugger-vscode/media/`.

- [ ] **Step 3: Commit the media update (extension repo)**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode
git add media/oidwindow.wasm media/oidwindow.js media/version.json
git commit -m "chore(media): rebuild WASM with persistence redesign"
```

---

### Task 11: Manual verification

No code; confirm behavioral parity and no regressions. Run the extension (F5) against a CodeLLDB session.

- [ ] **Step 1: Prefs persist globally** — change framerate / toggle contrast / move splitter, reload the VS Code window, reopen the viewer → settings restored.

- [ ] **Step 2: Buffers auto-replot per-workspace** — plot buffers, restart VS Code, start debugging, stop with those variables in scope → they auto-replot.

- [ ] **Step 3: Removal sticks** — remove a buffer from the viewer, continue/stop again → it does not come back; after restart it stays gone.

- [ ] **Step 4: Expiry** — a buffer not seen for >24 h is not replotted (can be checked with a temporary short expiry or by inspecting `workspaceState`).

- [ ] **Step 5: No regressions** — autocomplete in the viewer search box still lists in-scope symbols; manual plot (`oid.plot`) still works; `oid.watchOnStop` still seeds on first run.

- [ ] **Step 6: Desktop untouched** — run desktop OID (GDB/LLDB) and confirm its QSettings INI is still read/written normally.

---

## Self-review

| Spec section | Task |
|--------------|------|
| §2 Architecture (extension source of truth, two subsystems) | 2–10 |
| §3 Buffer persistence (seed, refresh, removal-diff, expiry, per-workspace) | 2, 3 |
| §4 UI-pref persistence (fields, type 6/7, SettingsApplier, skip QSettings, minmaxCompact omitted) | 4–9 |
| §5 Data model / storage keys / wire format / module layout | 2, 4, 5, 7 |
| §6 Regression safeguards (autocomplete, ordering, desktop) | 3, 8, 11 |
| §7 Error handling (invalid JSON, expiry, timeouts) | 2, 4, 8 |
| §8 Testing (ext unit, native codec, manual) | 2, 4, 5, 7, 11 |
| §9 Success criteria | 11 |
| Removal of old conflated module | 1 |

**Placeholder scan:** none — every code step contains full content.

**Type consistency:** `BufferStore` API (`startSession`/`names`/`recordHeld`/`reset`) consistent across Tasks 2–3; `SessionDeps` new fields (`beginBufferSession`/`wantedBuffers`/`captureHeld`) consistent across Task 3 impl + tests + extension wiring; `PrefStore` (`get`/`encode`/`applyDelta`) consistent across Tasks 4 + 6; wire JSON shape (`rendering.framerate`, `ui.*`) identical in TS pref-store (Task 4), TS tests (Task 5), and C++ codec (Task 7); `buildApplySessionState` / `sessionStateChanged` consistent across Tasks 5–6; `SessionStateFields` / `SessionStateExtraInputs` consistent across Tasks 7–9.
