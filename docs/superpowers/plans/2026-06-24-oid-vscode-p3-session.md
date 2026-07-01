# OID VS Code P3: Session management Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn P2's one-shot plot into a live session loop — populate the viewer's symbol autocomplete, let the viewer own the watched-buffer set, and re-plot it with fresh data on every debugger stop.

**Architecture:** Extension-only, on top of P2. Pure codecs and the session loop live in VS-Code-free modules (`ipc/message-exchange.ts`, `session/session-manager.ts`, `debugger/symbol-enumerator.ts`) so they unit-test under `node:test`. The VS-Code-coupled glue (`webview/panel.ts`'s `ViewerController`, `extension.ts`) is thin wiring verified by compile + manual run. The watched set is owned by the WASM viewer and polled via the existing `GetObservedSymbols` request/response protocol; `oid.watchOnStop` seeds it once per debug session.

**Tech Stack:** TypeScript (ES modules, `.js` import specifiers), `node:test` + `node:assert`, VS Code Debug API (DAP custom requests), Emscripten `wasm32` wire format.

## Global Constraints

- **Repo:** all changes are in the sibling extension repo `/Users/bruno/ws/openimagedebugger-vscode`. No OpenImageDebugger C++/WASM changes.
- **Wire format:** `wasm32` mode — message-type header, `size_t` counts, and string length prefixes are all little-endian `u32`. Strings are UTF-8, length-prefixed (no NUL terminator).
- **Message types** (`MessageType` enum, must match OID `src/ipc/message_exchange.h`): `GetObservedSymbols = 0`, `GetObservedSymbolsResponse = 1`, `SetAvailableSymbols = 2`, `PlotBufferContents = 3`, `PlotBufferRequest = 4`.
- **Module imports:** use `.js` specifiers (e.g. `'../src/ipc/message-exchange.js'`) — the project compiles to `out/` and runs the compiled JS.
- **Test layout:** specs live in `test/*.test.ts`; `npm test` runs `node --test out/test/*.js`. Modules that `import * as vscode` cannot load under `node:test`, so keep `session-manager.ts`, `message-exchange.ts`, and `symbol-enumerator.ts` free of any `vscode` import.
- **Build/test commands:** `npm run compile` (tsc), then `npm test`. Always compile before testing.
- **Persistence is out of scope** (deferred to P5). The observed set is in-memory only.

---

### Task 1: Symbol-list codecs in `message-exchange.ts`

Add the encoders for `SetAvailableSymbols` (host→viewer) and `GetObservedSymbols` (host→viewer, header-only), and a decoder for inbound viewer→extension frames (`GetObservedSymbolsResponse`, `PlotBufferRequest`).

**Files:**
- Modify: `openimagedebugger-vscode/src/ipc/message-exchange.ts`
- Test: `openimagedebugger-vscode/test/message-exchange.test.ts`

**Interfaces:**
- Consumes: existing module-private `pushU32`, `pushString`; existing `WasmLengthMode`.
- Produces:
  - `buildSetAvailableSymbols(names: string[], mode: WasmLengthMode): Uint8Array`
  - `buildGetObservedSymbols(): Uint8Array`
  - `decodeInbound(bytes: Uint8Array): InboundMessage`
  - `type InboundMessage = { kind: 'observedSymbols'; names: string[] } | { kind: 'plotRequest'; name: string } | { kind: 'unknown'; type: number }`
  - extended `MessageType` enum with all five members.

- [ ] **Step 1: Write the failing tests**

Append to `openimagedebugger-vscode/test/message-exchange.test.ts`:

```ts
import {
  buildSetAvailableSymbols,
  buildGetObservedSymbols,
  decodeInbound,
} from '../src/ipc/message-exchange.js';

// Local helper: encode an inbound frame the way the wasm32 viewer would.
function encodeFrame(type: number, strings: string[], withCount: boolean): Uint8Array {
  const parts: number[] = [];
  const u32 = (v: number) => parts.push(v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff);
  const str = (s: string) => {
    const b = new TextEncoder().encode(s);
    u32(b.length);
    for (const x of b) parts.push(x);
  };
  u32(type);
  if (withCount) u32(strings.length);
  for (const s of strings) str(s);
  return new Uint8Array(parts);
}

test('buildSetAvailableSymbols frames type 2, count, then names', () => {
  const bytes = buildSetAvailableSymbols(['img', 'mask'], 'wasm32');
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  assert.equal(view.getUint32(0, true), MessageType.SetAvailableSymbols);
  assert.equal(view.getUint32(4, true), 2); // count
  assert.equal(view.getUint32(8, true), 3); // length of 'img'
});

test('buildGetObservedSymbols is a 4-byte type-0 header', () => {
  const bytes = buildGetObservedSymbols();
  assert.equal(bytes.length, 4);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  assert.equal(view.getUint32(0, true), MessageType.GetObservedSymbols);
});

test('decodeInbound parses GetObservedSymbolsResponse names', () => {
  const frame = encodeFrame(MessageType.GetObservedSymbolsResponse, ['img', 'mask'], true);
  const msg = decodeInbound(frame);
  assert.deepEqual(msg, { kind: 'observedSymbols', names: ['img', 'mask'] });
});

test('decodeInbound parses an empty observed list', () => {
  const frame = encodeFrame(MessageType.GetObservedSymbolsResponse, [], true);
  assert.deepEqual(decodeInbound(frame), { kind: 'observedSymbols', names: [] });
});

test('decodeInbound parses a PlotBufferRequest name', () => {
  const frame = encodeFrame(MessageType.PlotBufferRequest, ['TestField'], false);
  assert.deepEqual(decodeInbound(frame), { kind: 'plotRequest', name: 'TestField' });
});

test('decodeInbound returns unknown for unhandled types', () => {
  const frame = encodeFrame(MessageType.PlotBufferContents, [], false);
  assert.deepEqual(decodeInbound(frame), { kind: 'unknown', type: MessageType.PlotBufferContents });
});
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile`
Expected: FAIL — `tsc` errors that `buildSetAvailableSymbols`, `buildGetObservedSymbols`, `decodeInbound` are not exported.

- [ ] **Step 3: Extend the `MessageType` enum**

In `openimagedebugger-vscode/src/ipc/message-exchange.ts`, replace:

```ts
export enum MessageType {
  PlotBufferContents = 3,
}
```

with:

```ts
export enum MessageType {
  GetObservedSymbols = 0,
  GetObservedSymbolsResponse = 1,
  SetAvailableSymbols = 2,
  PlotBufferContents = 3,
  PlotBufferRequest = 4,
}
```

- [ ] **Step 4: Add the encoders and decoder**

Append to the end of `openimagedebugger-vscode/src/ipc/message-exchange.ts`:

```ts
export function buildSetAvailableSymbols(
  names: string[],
  mode: WasmLengthMode
): Uint8Array {
  const parts: number[] = [];
  pushU32(parts, MessageType.SetAvailableSymbols);
  pushU32(parts, names.length);
  for (const name of names) {
    pushString(parts, name, mode);
  }
  return new Uint8Array(parts);
}

export function buildGetObservedSymbols(): Uint8Array {
  const parts: number[] = [];
  pushU32(parts, MessageType.GetObservedSymbols);
  return new Uint8Array(parts);
}

export type InboundMessage =
  | { kind: 'observedSymbols'; names: string[] }
  | { kind: 'plotRequest'; name: string }
  | { kind: 'unknown'; type: number };

export function decodeInbound(bytes: Uint8Array): InboundMessage {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let offset = 0;
  const readU32 = (): number => {
    const v = view.getUint32(offset, true);
    offset += 4;
    return v;
  };
  const readString = (): string => {
    const len = readU32();
    const slice = bytes.subarray(offset, offset + len);
    offset += len;
    return new TextDecoder().decode(slice);
  };

  const type = readU32();
  switch (type) {
    case MessageType.GetObservedSymbolsResponse: {
      const count = readU32();
      const names: string[] = [];
      for (let i = 0; i < count; i++) names.push(readString());
      return { kind: 'observedSymbols', names };
    }
    case MessageType.PlotBufferRequest:
      return { kind: 'plotRequest', name: readString() };
    default:
      return { kind: 'unknown', type };
  }
}
```

- [ ] **Step 5: Compile and run the tests to verify they pass**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile && npm test`
Expected: PASS — all `message-exchange` tests green, existing `PlotBufferContents` test still green.

- [ ] **Step 6: Commit**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode
git add src/ipc/message-exchange.ts test/message-exchange.test.ts
git commit -m "feat(ipc): symbol-list and observed-symbols codecs"
```

---

### Task 2: Scope symbol enumerator

Enumerate the current frame's local + argument variables (name + type) via DAP `scopes`/`variables`, for the viewer's autocomplete.

**Files:**
- Modify: `openimagedebugger-vscode/src/debugger/dap-client.ts`
- Create: `openimagedebugger-vscode/src/debugger/symbol-enumerator.ts`
- Test: `openimagedebugger-vscode/test/dap-client.test.ts` (extend)
- Test: `openimagedebugger-vscode/test/symbol-enumerator.test.ts` (new)

**Interfaces:**
- Consumes: existing `DapClient` / `DapSession.customRequest`.
- Produces:
  - In `dap-client.ts`: `interface DapScope { name: string; variablesReference: number; presentationHint?: string }`, `interface DapVariable { name: string; value: string; type?: string; variablesReference: number }`, and on `IDapClient`/`DapClient`: `scopes(frameId: number): Promise<DapScope[]>`, `variables(variablesReference: number): Promise<DapVariable[]>`.
  - In `symbol-enumerator.ts`: `interface ScopeReader { scopes(frameId): Promise<DapScope[]>; variables(ref): Promise<DapVariable[]> }`, `interface ScopeSymbol { name: string; type: string }`, `listScopeSymbols(reader: ScopeReader, frameId: number): Promise<ScopeSymbol[]>`.

- [ ] **Step 1: Write the failing tests**

Create `openimagedebugger-vscode/test/symbol-enumerator.test.ts`:

```ts
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import { listScopeSymbols, type ScopeReader } from '../src/debugger/symbol-enumerator.js';
import type { DapScope, DapVariable } from '../src/debugger/dap-client.js';

function reader(
  scopes: DapScope[],
  vars: Record<number, DapVariable[]>
): ScopeReader {
  return {
    async scopes() { return scopes; },
    async variables(ref) { return vars[ref] ?? []; },
  };
}

test('lists locals and arguments, skips registers/statics', async () => {
  const syms = await listScopeSymbols(
    reader(
      [
        { name: 'Local', variablesReference: 1, presentationHint: 'locals' },
        { name: 'Arguments', variablesReference: 2, presentationHint: 'arguments' },
        { name: 'Registers', variablesReference: 3, presentationHint: 'registers' },
      ],
      {
        1: [{ name: 'img', value: '...', type: 'cv::Mat', variablesReference: 0 }],
        2: [{ name: 'rows', value: '4', type: 'int', variablesReference: 0 }],
        3: [{ name: 'rax', value: '0x0', variablesReference: 0 }],
      }
    ),
    7
  );
  assert.deepEqual(syms, [
    { name: 'img', type: 'cv::Mat' },
    { name: 'rows', type: 'int' },
  ]);
});

test('falls back to scope name when no presentationHint, and dedupes', async () => {
  const syms = await listScopeSymbols(
    reader(
      [
        { name: 'Local', variablesReference: 1 },
        { name: 'Local', variablesReference: 2 },
      ],
      {
        1: [{ name: 'img', value: '', type: 'cv::Mat', variablesReference: 0 }],
        2: [
          { name: 'img', value: '', type: 'cv::Mat', variablesReference: 0 },
          { name: '', value: '', variablesReference: 0 },
        ],
      }
    ),
    1
  );
  assert.deepEqual(syms, [{ name: 'img', type: 'cv::Mat' }]);
});
```

Append to `openimagedebugger-vscode/test/dap-client.test.ts`:

```ts
import { DapClient } from '../src/debugger/dap-client.js';

test('scopes/variables unwrap the DAP response and default to []', async () => {
  const calls: Array<[string, any]> = [];
  const session = {
    async customRequest(command: string, args?: unknown) {
      calls.push([command, args]);
      if (command === 'scopes') return { scopes: [{ name: 'Local', variablesReference: 9 }] };
      if (command === 'variables') return {}; // missing variables array
      return {};
    },
  };
  const client = new DapClient(session);
  assert.deepEqual(await client.scopes(7), [{ name: 'Local', variablesReference: 9 }]);
  assert.deepEqual(await client.variables(9), []);
  assert.deepEqual(await client.variables(0), []); // ref <= 0 short-circuits
  assert.deepEqual(calls, [['scopes', { frameId: 7 }], ['variables', { variablesReference: 9 }]]);
});
```

(If `dap-client.test.ts` does not already import `test`/`assert`, add `import { strict as assert } from 'node:assert';` and `import { test } from 'node:test';` at the top.)

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile`
Expected: FAIL — `symbol-enumerator.js` not found; `scopes`/`variables` not on `DapClient`.

- [ ] **Step 3: Add `scopes`/`variables` to the DAP client**

In `openimagedebugger-vscode/src/debugger/dap-client.ts`, after the `EvaluateResult` interface (line 10), add:

```ts
export interface DapScope {
  name: string;
  variablesReference: number;
  presentationHint?: string;
}

export interface DapVariable {
  name: string;
  value: string;
  type?: string;
  variablesReference: number;
}
```

Add these two methods to the `IDapClient` interface:

```ts
  scopes(frameId: number): Promise<DapScope[]>;
  variables(variablesReference: number): Promise<DapVariable[]>;
```

Add these two methods to the `DapClient` class (after `topFrameId`):

```ts
  async scopes(frameId: number): Promise<DapScope[]> {
    const res = await this.session.customRequest('scopes', { frameId });
    return res?.scopes ?? [];
  }

  async variables(variablesReference: number): Promise<DapVariable[]> {
    if (variablesReference <= 0) return [];
    const res = await this.session.customRequest('variables', { variablesReference });
    return res?.variables ?? [];
  }
```

- [ ] **Step 4: Create the enumerator**

Create `openimagedebugger-vscode/src/debugger/symbol-enumerator.ts`:

```ts
import type { DapScope, DapVariable } from './dap-client.js';

export interface ScopeReader {
  scopes(frameId: number): Promise<DapScope[]>;
  variables(variablesReference: number): Promise<DapVariable[]>;
}

export interface ScopeSymbol {
  name: string;
  type: string;
}

function isLocalOrArgument(scope: DapScope): boolean {
  const hint = scope.presentationHint?.toLowerCase();
  if (hint === 'locals' || hint === 'arguments') return true;
  if (hint === 'registers') return false;
  return /local|argument/i.test(scope.name);
}

/** Flat list of the current frame's local + argument variables (top level only). */
export async function listScopeSymbols(
  reader: ScopeReader,
  frameId: number
): Promise<ScopeSymbol[]> {
  const scopes = await reader.scopes(frameId);
  const seen = new Set<string>();
  const out: ScopeSymbol[] = [];
  for (const scope of scopes) {
    if (!isLocalOrArgument(scope)) continue;
    const vars = await reader.variables(scope.variablesReference);
    for (const v of vars) {
      if (!v.name || seen.has(v.name)) continue;
      seen.add(v.name);
      out.push({ name: v.name, type: v.type ?? '' });
    }
  }
  return out;
}
```

- [ ] **Step 5: Compile and run the tests to verify they pass**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile && npm test`
Expected: PASS — `symbol-enumerator` and new `dap-client` tests green; all prior tests still green.

- [ ] **Step 6: Commit**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode
git add src/debugger/dap-client.ts src/debugger/symbol-enumerator.ts test/symbol-enumerator.test.ts test/dap-client.test.ts
git commit -m "feat(debugger): enumerate local+argument scope symbols"
```

---

### Task 3: Session loop in `SessionManager`

Replace the static watch-list `onStopped` with the full loop: push available symbols, poll the viewer's observed set, seed once from `oid.watchOnStop`, re-plot each observed buffer. Add `onPlotBufferRequest` and `resetSeed`.

**Files:**
- Modify: `openimagedebugger-vscode/src/session/session-manager.ts`
- Test: `openimagedebugger-vscode/test/session-manager.test.ts`

**Interfaces:**
- Consumes: existing `PlotBufferParams`.
- Produces:
  - `interface ViewerChannel { setAvailableSymbols(names: string[]): void | Promise<void>; getObservedSymbols(): Promise<string[]> }`
  - extended `SessionDeps` adding `channel: ViewerChannel` and `listScopeSymbols: (frameId: number) => Promise<string[]>`.
  - on `SessionManager`: `resetSeed(): void`, `onPlotBufferRequest(name: string, threadId: number): Promise<void>`, and the rewritten `onStopped(threadId: number): Promise<void>`.

- [ ] **Step 1: Write the failing tests**

Replace the entire contents of `openimagedebugger-vscode/test/session-manager.test.ts` with:

```ts
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import { SessionManager, type SessionDeps } from '../src/session/session-manager.js';
import type { PlotBufferParams } from '../src/ipc/message-exchange.js';

function params(name: string): PlotBufferParams {
  return {
    variableName: name,
    displayName: name,
    pixelLayout: 'rgba',
    transpose: false,
    width: 1,
    height: 1,
    channels: 1,
    stride: 1,
    bufferType: 0,
    pixels: new Uint8Array([1]),
  };
}

function harness(overrides: Partial<SessionDeps> = {}) {
  const sent: string[] = [];
  const warnings: string[] = [];
  const available: string[][] = [];
  let observed: string[] = [];
  const deps: SessionDeps = {
    source: {
      async getBufferMetadata(name) {
        if (name === 'bad') throw new Error('not Mat');
        return params(name);
      },
    },
    sink: {
      async ensureReady() {},
      sendPlotBuffer(bytes) { sent.push(String(bytes.length)); },
    },
    channel: {
      setAvailableSymbols(names) { available.push(names); },
      async getObservedSymbols() { return observed; },
    },
    encode: (p) => new Uint8Array([p.bufferType]),
    resolveFrame: async () => 42,
    listScopeSymbols: async () => ['a', 'b'],
    getWatchList: () => [],
    warn: (msg) => warnings.push(msg),
    ...overrides,
  };
  return {
    deps, sent, warnings, available,
    setObserved: (o: string[]) => { observed = o; },
    mgr: new SessionManager(deps),
  };
}

test('onStopped pushes enumerated symbols to the viewer autocomplete', async () => {
  const { available, mgr } = harness();
  await mgr.onStopped(1);
  assert.deepEqual(available[0], ['a', 'b']);
});

test('onStopped replots the viewer-observed set', async () => {
  const h = harness();
  h.setObserved(['img', 'mask']);
  await h.mgr.onStopped(1);
  assert.equal(h.sent.length, 2);
});

test('watchOnStop seeds the observed set only on the first stop', async () => {
  const { sent, mgr } = harness({ getWatchList: () => ['img'] });
  await mgr.onStopped(1); // seeds: plots img
  await mgr.onStopped(1); // already seeded, channel still []: no new plots
  assert.equal(sent.length, 1);
});

test('resetSeed re-arms the watchOnStop seed', async () => {
  const { sent, mgr } = harness({ getWatchList: () => ['img'] });
  await mgr.onStopped(1);
  mgr.resetSeed();
  await mgr.onStopped(1);
  assert.equal(sent.length, 2);
});

test('seed is unioned with observed without duplicating', async () => {
  const h = harness({ getWatchList: () => ['img', 'mask'] });
  h.setObserved(['img']);
  await h.mgr.onStopped(1);
  assert.equal(h.sent.length, 2); // img (observed) + mask (seed); img not doubled
});

test('a failing buffer is isolated and warned', async () => {
  const h = harness({ getWatchList: () => ['img', 'bad', 'mask'] });
  await h.mgr.onStopped(1);
  assert.equal(h.sent.length, 2);
  assert.equal(h.warnings.length, 1);
  assert(h.warnings[0].includes('bad'));
});

test('onPlotBufferRequest plots a single buffer', async () => {
  const { sent, mgr } = harness();
  await mgr.onPlotBufferRequest('img', 1);
  assert.equal(sent.length, 1);
});

test('plotVariable sends a single buffer', async () => {
  const { sent, mgr } = harness();
  await mgr.plotVariable('img', 1);
  assert.equal(sent.length, 1);
});
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile`
Expected: FAIL — `SessionDeps` has no `channel`/`listScopeSymbols`; `resetSeed`/`onPlotBufferRequest` missing.

- [ ] **Step 3: Rewrite `session-manager.ts`**

Replace the entire contents of `openimagedebugger-vscode/src/session/session-manager.ts` with:

```ts
import type { PlotBufferParams } from '../ipc/message-exchange.js';

export interface MetadataSource {
  getBufferMetadata(variable: string, frameId: number): Promise<PlotBufferParams>;
}

export interface PlotSink {
  ensureReady(): Promise<void>;
  sendPlotBuffer(bytes: Uint8Array): void | Promise<void>;
}

export interface ViewerChannel {
  setAvailableSymbols(names: string[]): void | Promise<void>;
  getObservedSymbols(): Promise<string[]>;
}

export interface SessionDeps {
  source: MetadataSource;
  sink: PlotSink;
  channel: ViewerChannel;
  encode: (p: PlotBufferParams) => Uint8Array;
  resolveFrame: (threadId: number) => Promise<number>;
  listScopeSymbols: (frameId: number) => Promise<string[]>;
  getWatchList: () => string[];
  warn: (message: string) => void;
}

function describe(err: unknown): string {
  return err instanceof Error ? err.message : String(err);
}

function union(a: string[], b: string[]): string[] {
  const seen = new Set(a);
  const out = [...a];
  for (const x of b) {
    if (!seen.has(x)) {
      seen.add(x);
      out.push(x);
    }
  }
  return out;
}

export class SessionManager {
  private seeded = false;

  constructor(private readonly deps: SessionDeps) {}

  /** Re-arm the one-time watchOnStop seed (call when a new debug session starts). */
  resetSeed(): void {
    this.seeded = false;
  }

  async plotVariable(name: string, threadId: number): Promise<void> {
    const frameId = await this.deps.resolveFrame(threadId);
    await this.plotAtFrame(name, frameId);
  }

  /** Handle a viewer-initiated PlotBufferRequest (user added a symbol in the viewer). */
  async onPlotBufferRequest(name: string, threadId: number): Promise<void> {
    await this.plotVariable(name, threadId);
  }

  async onStopped(threadId: number): Promise<void> {
    const frameId = await this.deps.resolveFrame(threadId);

    // 1. Populate the viewer's autocomplete with in-scope names.
    try {
      const names = await this.deps.listScopeSymbols(frameId);
      await this.deps.channel.setAvailableSymbols(names);
    } catch (err) {
      this.deps.warn(`OID: could not list symbols: ${describe(err)}`);
    }

    // 2. Ask the viewer which buffers it is watching (it owns the list).
    let observed: string[];
    try {
      observed = await this.deps.channel.getObservedSymbols();
    } catch {
      observed = [];
    }

    // 3. First stop of the session: seed from oid.watchOnStop config.
    if (!this.seeded) {
      observed = union(observed, this.deps.getWatchList());
      this.seeded = true;
    }

    // 4. Re-plot every observed buffer with fresh data.
    for (const name of observed) {
      await this.plotAtFrame(name, frameId);
    }
  }

  private async plotAtFrame(name: string, frameId: number): Promise<void> {
    try {
      const p = await this.deps.source.getBufferMetadata(name, frameId);
      await this.deps.sink.ensureReady();
      await this.deps.sink.sendPlotBuffer(this.deps.encode(p));
    } catch (err) {
      this.deps.warn(`OID: could not plot '${name}': ${describe(err)}`);
    }
  }
}
```

- [ ] **Step 4: Compile and run the tests to verify they pass**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile && npm test`
Expected: PASS — all 8 `session-manager` tests green. (`extension.ts` will not yet compile against the new deps — that is fixed in Task 4; if `npm run compile` fails only on `src/extension.ts`, that is expected here. Run `npm test` regardless to confirm the session-manager unit tests pass against the freshly built `out/`.)

> Note: because Task 3 changes `SessionDeps`, `src/extension.ts` (Task 4) is now out of sync. If `tsc` blocks `out/` from updating, temporarily it is acceptable to proceed directly to Task 4 in the same sitting; do not commit Task 3 until `npm test` shows the session-manager suite green. The committed tree need not compile mid-task, but SHOULD compile by the end of Task 4.

- [ ] **Step 5: Commit**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode
git add src/session/session-manager.ts test/session-manager.test.ts
git commit -m "feat(session): viewer-owned watch loop with available-symbols push"
```

---

### Task 4: Viewer channel + extension wiring

Make `ViewerController` implement `ViewerChannel` (send `SetAvailableSymbols`, poll `GetObservedSymbols` with a timeout, dispatch inbound `oid-ipc-out` frames) and wire it all in `extension.ts`. This is the integration task; verified by compile + the existing unit suite + the manual matrix.

**Files:**
- Modify: `openimagedebugger-vscode/src/webview/panel.ts`
- Modify: `openimagedebugger-vscode/src/extension.ts`

**Interfaces:**
- Consumes: `buildSetAvailableSymbols`, `buildGetObservedSymbols`, `decodeInbound` (Task 1); `SessionManager`, `ViewerChannel` (Task 3); `listScopeSymbols` (Task 2).
- Produces: `ViewerController` gains `setAvailableSymbols(names: string[]): Promise<void>`, `getObservedSymbols(): Promise<string[]>`, `onPlotBufferRequest(handler: (name: string) => void): void` — satisfying `ViewerChannel` and supplying the inbound callback.

- [ ] **Step 1: Add the channel + inbound dispatch to `ViewerController`**

In `openimagedebugger-vscode/src/webview/panel.ts`, update the import line near the top (after the `vscode` import) to add the codecs:

```ts
import {
  buildSetAvailableSymbols,
  buildGetObservedSymbols,
  decodeInbound,
} from '../ipc/message-exchange.js';
```

Add these fields to the `ViewerController` class (next to the existing `private ready = false;`):

```ts
  private observedResolver: ((names: string[]) => void) | undefined;
  private observedTimer: ReturnType<typeof setTimeout> | undefined;
  private plotRequestHandler: ((name: string) => void) | undefined;
```

Replace the `panel.webview.onDidReceiveMessage(...)` call inside `open()` with:

```ts
    panel.webview.onDidReceiveMessage((msg) => {
      if (msg?.type === 'oid-control' && msg.event === 'viewer-ready') {
        this.ready = true;
        for (const w of this.readyWaiters.splice(0)) w();
        return;
      }
      if (msg?.type === 'oid-ipc-out' && msg.payload) {
        this.handleInbound(new Uint8Array(msg.payload));
      }
    });
```

Add `this.resolveObserved([]);` inside the existing `panel.onDidDispose(() => { ... })` callback (so a pending poll never hangs after the panel closes), e.g.:

```ts
    panel.onDidDispose(() => {
      this.panel = undefined;
      this.ready = false;
      this.resolveObserved([]);
      for (const w of this.readyWaiters.splice(0)) w();
    });
```

Add these methods to the `ViewerController` class (after `sendPlotBuffer`):

```ts
  onPlotBufferRequest(handler: (name: string) => void): void {
    this.plotRequestHandler = handler;
  }

  async setAvailableSymbols(names: string[]): Promise<void> {
    await this.ensureReady();
    this.panel?.webview.postMessage({
      type: 'oid-ipc-forward',
      payload: Array.from(buildSetAvailableSymbols(names, 'wasm32')),
    });
  }

  async getObservedSymbols(): Promise<string[]> {
    await this.ensureReady();
    // Settle any prior in-flight poll defensively (only one poll at a time).
    this.resolveObserved([]);
    const result = new Promise<string[]>((resolve) => {
      this.observedResolver = resolve;
      this.observedTimer = setTimeout(() => {
        console.warn('OID: GetObservedSymbols timed out; treating as empty.');
        this.resolveObserved([]);
      }, 1000);
    });
    this.panel?.webview.postMessage({
      type: 'oid-ipc-forward',
      payload: Array.from(buildGetObservedSymbols()),
    });
    return result;
  }

  private handleInbound(bytes: Uint8Array): void {
    const inbound = decodeInbound(bytes);
    if (inbound.kind === 'observedSymbols') {
      this.resolveObserved(inbound.names);
    } else if (inbound.kind === 'plotRequest') {
      this.plotRequestHandler?.(inbound.name);
    }
  }

  private resolveObserved(names: string[]): void {
    if (this.observedTimer) {
      clearTimeout(this.observedTimer);
      this.observedTimer = undefined;
    }
    const resolve = this.observedResolver;
    this.observedResolver = undefined;
    resolve?.(names);
  }
```

- [ ] **Step 2: Wire the new deps + callbacks in `extension.ts`**

In `openimagedebugger-vscode/src/extension.ts`, add the enumerator import after the existing `DapClient` import (line 4):

```ts
import { listScopeSymbols } from './debugger/symbol-enumerator.js';
```

Add the `channel` and `listScopeSymbols` deps to the `new SessionManager({ ... })` object (alongside `sink: viewer,`):

```ts
    sink: viewer,
    channel: viewer,
    listScopeSymbols: async (frameId) => {
      const session = activeSession();
      if (!session) throw new Error('no active debug session');
      const syms = await listScopeSymbols(new DapClient(session), frameId);
      return syms.map((s) => s.name);
    },
```

Immediately after the `const manager = new SessionManager({ ... });` block (before the DebugAdapterTracker registration), add the inbound handler and the per-session seed reset:

```ts
  viewer.onPlotBufferRequest((name) => {
    if (stoppedThreadId === undefined) {
      vscode.window.showWarningMessage('OID: pause in a debug session first.');
      return;
    }
    void manager.onPlotBufferRequest(name, stoppedThreadId);
  });

  context.subscriptions.push(
    vscode.debug.onDidStartDebugSession(() => manager.resetSeed())
  );
```

- [ ] **Step 3: Compile and run the full test suite**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile && npm test`
Expected: PASS — `tsc` produces no errors (including `src/extension.ts`); all unit suites green.

- [ ] **Step 4: Manual verification (CodeLLDB → WASM viewer)**

Prerequisites: OID testbench built; extension media (`oidwindow.js/.wasm`, `qtloader.js`) present in `media/`; CodeLLDB installed.

1. Launch the Extension Development Host (F5) with a `launch.json` that debugs the OID `testbench` binary via CodeLLDB.
2. Break in `testbench/main.cpp` inside `nest()`. Open the viewer (`OID: Open Viewer Panel`).
   - Expect: the viewer's symbol search autocomplete lists `nest()`'s locals + arguments.
3. In the viewer's symbol box, type `TestField` and pick it.
   - Expect: `TestField` plots (viewer-initiated `PlotBufferRequest`).
4. Set `"oid.watchOnStop": ["TestField"]`, restart the debug session, hit the breakpoint.
   - Expect: `TestField` auto-plots on the first stop.
5. Step/continue so values change.
   - Expect: watched buffers re-plot with updated data.
6. Remove `TestField` from the viewer's list, then step.
   - Expect: `TestField` is **not** re-plotted (poll returns the smaller observed set).

- [ ] **Step 5: Commit**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode
git add src/webview/panel.ts src/extension.ts
git commit -m "feat(session): viewer channel + extension wiring for P3 session loop"
```

---

## Plan self-review

- **Spec coverage:**
  - §3 `symbol-enumerator.ts` → Task 2. `SetAvailableSymbols` encode → Task 1; sent in loop → Task 3 (`setAvailableSymbols`) + Task 4 (`ViewerController.setAvailableSymbols`).
  - §3/§4 `GetObservedSymbols` poll + `GetObservedSymbolsResponse` decode → Task 1 (codec) + Task 4 (poll/correlation in `ViewerController`).
  - §3/§4 `PlotBufferRequest` inbound → Task 1 (decode) + Task 4 (`onPlotBufferRequest` dispatch + extension guard) + Task 3 (`onPlotBufferRequest`).
  - §4 auto-replot of observed set + §2 first-stop `oid.watchOnStop` seed → Task 3 (`onStopped`, `union`, `seeded`, `resetSeed`).
  - §5 request/response correlation + 1000 ms timeout → Task 4 (`getObservedSymbols`/`resolveObserved`).
  - §6 error handling (enumeration failure, observed timeout, per-buffer skip) → Task 3 try/catch + Task 4 timeout; covered by session-manager tests.
  - §8 persistence deferred → no task (intentional, per Global Constraints).
- **Placeholder scan:** none — every code step shows full content; every command shows expected result.
- **Type consistency:** `ViewerChannel` (Task 3) matches `ViewerController`'s `setAvailableSymbols`/`getObservedSymbols` (Task 4). `ScopeReader` (Task 2) is structurally satisfied by `DapClient.scopes`/`variables` (Task 2). `InboundMessage` kinds `'observedSymbols'`/`'plotRequest'`/`'unknown'` are produced in Task 1 and consumed in Task 4. `MessageType` numbers match OID `message_exchange.h`. `listScopeSymbols` returns `ScopeSymbol[]`; the extension dep maps to `string[]` for `SessionDeps.listScopeSymbols`, matching its `Promise<string[]>` signature.
