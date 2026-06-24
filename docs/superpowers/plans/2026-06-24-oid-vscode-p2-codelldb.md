# OID VS Code P2: CodeLLDB → WASM viewer bridge — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Plot a live OpenCV `cv::Mat` from a CodeLLDB debug session into the existing WASM viewer, replacing the hardcoded smoke-test buffer with a real CodeLLDB → metadata → pixels → IPC pipeline.

**Architecture:** Pure functions decode `cv::Mat` flags into buffer metadata; a thin DAP client issues `evaluate`/`readMemory`; a bridge orchestrates one plot; a session manager runs the watch list on `stopped`; a viewer controller forwards the existing legacy `PlotBufferContents` binary frame to the webview. Extension-only — no OID C++/WASM repo changes.

**Tech Stack:** TypeScript 5.4, VS Code Extension API (Debug `customRequest`), Node 16 module resolution, `node:test` + `node:assert`.

**Design spec:** `docs/superpowers/specs/2026-06-24-oid-vscode-p2-codelldb-design.md`

## Global Constraints

- **Repo:** all paths and `git` commands run in `openimagedebugger-vscode/` (sibling of the OID repo). Paths below are relative to that repo root.
- **Module system:** `module: Node16` — every relative import MUST carry a `.js` extension (e.g. `from '../ipc/message-exchange.js'`), even in `.ts` source.
- **Tests:** `node:test` + `node:assert/strict` only. Run via `npm run compile && npm test` (`tsc -p .` then `node --test out/test/*.js`). No extra test deps.
- **Wire format:** legacy binary `PlotBufferContents` via the existing `buildPlotBufferContents(params, 'wasm32')`. Do not introduce OBP/protobuf.
- **Debug type:** CodeLLDB registers as debug type `lldb`.
- **OID buffer-type values** (mirror OpenCV depth codes): `UINT8=0, UINT16=2, INT16=3, INT32=4, FLOAT32=5, FLOAT64=6`.
- **`transpose` is always `false`** in P2.

---

## File map

| File | Role |
|------|------|
| `src/typebridge/buffer-metadata.ts` | `bytesPerChannel(typeValue)` helper; OID type constants |
| `src/typebridge/opencv-mat.ts` | Pure `decodeMat(fields) → MatMetadata` (flags decode) |
| `src/debugger/dap-client.ts` | `IDapClient` + `DapClient` over a debug session; `parseIntResult` |
| `src/debugger/codelldb-bridge.ts` | `CodeLldbBridge.getBufferMetadata(variable, frameId) → PlotBufferParams` |
| `src/session/session-manager.ts` | `SessionManager` — watch loop + `plotVariable` (injected deps) |
| `src/webview/panel.ts` | `ViewerController` (singleton panel, `sendPlotBuffer`, smoke-test off by default) |
| `src/extension.ts` | Commands `oid.plot`/`oid.openPanel`, `lldb` tracker, `oid.watchOnStop` |
| `package.json` | Manifest: commands, `onDebug`, `oid.watchOnStop` config |

---

### Task 1: Buffer-type size helper

**Files:**
- Create: `src/typebridge/buffer-metadata.ts`
- Test: `test/buffer-metadata.test.ts`

**Interfaces:**
- Produces: `enum OidBufferType`; `function bytesPerChannel(typeValue: number): number`

- [ ] **Step 1: Write the failing test**

```typescript
// test/buffer-metadata.test.ts
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import { OidBufferType, bytesPerChannel } from '../src/typebridge/buffer-metadata.js';

test('bytesPerChannel maps OID buffer types to byte widths', () => {
  assert.equal(bytesPerChannel(OidBufferType.Uint8), 1);
  assert.equal(bytesPerChannel(OidBufferType.Uint16), 2);
  assert.equal(bytesPerChannel(OidBufferType.Int16), 2);
  assert.equal(bytesPerChannel(OidBufferType.Int32), 4);
  assert.equal(bytesPerChannel(OidBufferType.Float32), 4);
  assert.equal(bytesPerChannel(OidBufferType.Float64), 8);
});

test('bytesPerChannel defaults unknown depth codes to 1 byte', () => {
  assert.equal(bytesPerChannel(1), 1); // CV_8S — unsupported, treated as 1 byte
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `npm run compile`
Expected: FAIL — `Cannot find module '../src/typebridge/buffer-metadata.js'`

- [ ] **Step 3: Write minimal implementation**

```typescript
// src/typebridge/buffer-metadata.ts

/** OID buffer type values, mirroring OpenCV depth codes (CV_8U=0 ... CV_64F=6). */
export enum OidBufferType {
  Uint8 = 0,
  Uint16 = 2,
  Int16 = 3,
  Int32 = 4,
  Float32 = 5,
  Float64 = 6,
}

/** Bytes per channel for an OpenCV depth code (flags & 7). Unknown codes → 1. */
export function bytesPerChannel(typeValue: number): number {
  switch (typeValue) {
    case OidBufferType.Uint16:
    case OidBufferType.Int16:
      return 2;
    case OidBufferType.Int32:
    case OidBufferType.Float32:
      return 4;
    case OidBufferType.Float64:
      return 8;
    default:
      return 1;
  }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `npm run compile && npm test`
Expected: PASS (this file's tests + existing `message-exchange.test.ts`)

- [ ] **Step 5: Commit**

```bash
git add src/typebridge/buffer-metadata.ts test/buffer-metadata.test.ts
git commit -m "feat(typebridge): add OID buffer-type size helper"
```

---

### Task 2: OpenCV Mat flags decode

**Files:**
- Create: `src/typebridge/opencv-mat.ts`
- Test: `test/opencv-mat.test.ts`

**Interfaces:**
- Consumes: `bytesPerChannel` from `buffer-metadata.js`
- Produces:
  - `interface MatFields { flags: number; cols: number; rows: number; step0: number }`
  - `interface MatMetadata { width: number; height: number; channels: number; bufferType: number; stride: number; pixelLayout: 'bgra' | 'rgba'; byteCount: number }`
  - `function decodeMat(f: MatFields): MatMetadata`

- [ ] **Step 1: Write the failing test**

```typescript
// test/opencv-mat.test.ts
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import { decodeMat } from '../src/typebridge/opencv-mat.js';

const MAGIC = 0x42ff0000; // Mat::MAGIC_VAL high bits; ignored by the decode masks

test('decodeMat handles CV_8UC3 (BGR, 1 byte/channel)', () => {
  const m = decodeMat({ flags: MAGIC | 16, cols: 4, rows: 2, step0: 12 });
  assert.equal(m.channels, 3);
  assert.equal(m.bufferType, 0);            // uint8
  assert.equal(m.width, 4);
  assert.equal(m.height, 2);
  assert.equal(m.stride, 4);                // 12 / 3 / 1
  assert.equal(m.pixelLayout, 'bgra');
  assert.equal(m.byteCount, 24);            // rows * step0
});

test('decodeMat handles CV_8UC1 (grayscale)', () => {
  const m = decodeMat({ flags: MAGIC | 0, cols: 5, rows: 3, step0: 5 });
  assert.equal(m.channels, 1);
  assert.equal(m.bufferType, 0);
  assert.equal(m.stride, 5);
  assert.equal(m.pixelLayout, 'rgba');
});

test('decodeMat handles CV_32FC1 (float, 4 bytes/channel)', () => {
  const m = decodeMat({ flags: MAGIC | 5, cols: 4, rows: 2, step0: 16 });
  assert.equal(m.channels, 1);
  assert.equal(m.bufferType, 5);            // float32
  assert.equal(m.stride, 4);                // 16 / 1 / 4
  assert.equal(m.byteCount, 32);
});

test('decodeMat handles CV_16UC1 (2 bytes/channel)', () => {
  const m = decodeMat({ flags: MAGIC | 2, cols: 3, rows: 1, step0: 6 });
  assert.equal(m.bufferType, 2);            // uint16
  assert.equal(m.stride, 3);                // 6 / 1 / 2
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `npm run compile`
Expected: FAIL — `Cannot find module '../src/typebridge/opencv-mat.js'`

- [ ] **Step 3: Write minimal implementation**

```typescript
// src/typebridge/opencv-mat.ts
import { bytesPerChannel } from './buffer-metadata.js';

const CV_CN_SHIFT = 3;
const CV_CN_MAX = 512;
const CV_MAT_CN_MASK = (CV_CN_MAX - 1) << CV_CN_SHIFT;
const CV_DEPTH_MAX = 1 << CV_CN_SHIFT;
const CV_MAT_TYPE_MASK = CV_DEPTH_MAX * CV_CN_MAX - 1;

export interface MatFields {
  flags: number;
  cols: number;
  rows: number;
  step0: number; // step.buf[0]: row stride in bytes
}

export interface MatMetadata {
  width: number;
  height: number;
  channels: number;
  bufferType: number; // flags & 7 (OpenCV depth code)
  stride: number;     // row stride in elements
  pixelLayout: 'bgra' | 'rgba';
  byteCount: number;  // rows * step0
}

/** Port of resources/oidscripts/oidtypes/opencv.py Mat.get_buffer_metadata. */
export function decodeMat(f: MatFields): MatMetadata {
  const channels = ((f.flags & CV_MAT_CN_MASK) >> CV_CN_SHIFT) + 1;
  const bufferType = (f.flags & CV_MAT_TYPE_MASK) & 7;
  const stride = Math.floor(
    Math.floor(f.step0 / channels) / bytesPerChannel(bufferType)
  );
  return {
    width: f.cols,
    height: f.rows,
    channels,
    bufferType,
    stride,
    pixelLayout: channels >= 3 ? 'bgra' : 'rgba',
    byteCount: f.rows * f.step0,
  };
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `npm run compile && npm test`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/typebridge/opencv-mat.ts test/opencv-mat.test.ts
git commit -m "feat(typebridge): decode cv::Mat flags into buffer metadata"
```

---

### Task 3: DAP client

**Files:**
- Create: `src/debugger/dap-client.ts`
- Test: `test/dap-client.test.ts`

**Interfaces:**
- Produces:
  - `interface DapSession { customRequest(command: string, args?: unknown): Promise<any> }`
  - `interface EvaluateResult { result: string; memoryReference?: string; variablesReference: number; type?: string }`
  - `interface IDapClient { evaluate(expression: string, frameId: number): Promise<EvaluateResult>; readMemory(memoryReference: string, count: number): Promise<Uint8Array>; topFrameId(threadId: number): Promise<number> }`
  - `class DapClient implements IDapClient` (constructor `(session: DapSession)`)
  - `function parseIntResult(result: string): number`

- [ ] **Step 1: Write the failing test**

```typescript
// test/dap-client.test.ts
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import { DapClient, parseIntResult } from '../src/debugger/dap-client.js';

test('parseIntResult parses decimal and hex CodeLLDB results', () => {
  assert.equal(parseIntResult('640'), 640);
  assert.equal(parseIntResult(' 0x10 '), 16);
  assert.equal(parseIntResult('0X42ff0003'), 0x42ff0003);
});

test('readMemory decodes base64 DAP payloads to bytes', async () => {
  const session = {
    async customRequest(command: string, args: any) {
      assert.equal(command, 'readMemory');
      assert.equal(args.memoryReference, '0x1000');
      assert.equal(args.count, 3);
      return { address: '0x1000', data: Buffer.from([1, 2, 3]).toString('base64') };
    },
  };
  const bytes = await new DapClient(session).readMemory('0x1000', 3);
  assert.deepEqual([...bytes], [1, 2, 3]);
});

test('topFrameId returns the first stack frame id', async () => {
  const session = {
    async customRequest(command: string, args: any) {
      assert.equal(command, 'stackTrace');
      assert.equal(args.threadId, 7);
      return { stackFrames: [{ id: 42 }, { id: 43 }] };
    },
  };
  assert.equal(await new DapClient(session).topFrameId(7), 42);
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `npm run compile`
Expected: FAIL — `Cannot find module '../src/debugger/dap-client.js'`

- [ ] **Step 3: Write minimal implementation**

```typescript
// src/debugger/dap-client.ts

export interface DapSession {
  customRequest(command: string, args?: unknown): Promise<any>;
}

export interface EvaluateResult {
  result: string;
  memoryReference?: string;
  variablesReference: number;
  type?: string;
}

export interface IDapClient {
  evaluate(expression: string, frameId: number): Promise<EvaluateResult>;
  readMemory(memoryReference: string, count: number): Promise<Uint8Array>;
  topFrameId(threadId: number): Promise<number>;
}

/** Parse a CodeLLDB scalar `evaluate` result string (decimal or 0x-hex) to a number. */
export function parseIntResult(result: string): number {
  const s = result.trim();
  return /^0x/i.test(s) ? parseInt(s, 16) : parseInt(s, 10);
}

export class DapClient implements IDapClient {
  constructor(private readonly session: DapSession) {}

  evaluate(expression: string, frameId: number): Promise<EvaluateResult> {
    return this.session.customRequest('evaluate', {
      expression,
      frameId,
      context: 'watch',
    });
  }

  async readMemory(memoryReference: string, count: number): Promise<Uint8Array> {
    if (count <= 0) return new Uint8Array(0);
    const res = await this.session.customRequest('readMemory', {
      memoryReference,
      count,
    });
    if (!res?.data) return new Uint8Array(0);
    return new Uint8Array(Buffer.from(res.data, 'base64'));
  }

  async topFrameId(threadId: number): Promise<number> {
    const res = await this.session.customRequest('stackTrace', {
      threadId,
      startFrame: 0,
      levels: 1,
    });
    return res.stackFrames[0].id;
  }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `npm run compile && npm test`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/debugger/dap-client.ts test/dap-client.test.ts
git commit -m "feat(debugger): add CodeLLDB DAP client wrappers"
```

---

### Task 4: CodeLLDB bridge

**Files:**
- Create: `src/debugger/codelldb-bridge.ts`
- Test: `test/codelldb-bridge.test.ts`
- Test fixture: `test/fixtures/cv-mat-variables.json`

**Interfaces:**
- Consumes: `IDapClient`, `EvaluateResult`, `parseIntResult` from `dap-client.js`; `decodeMat` from `opencv-mat.js`; `PlotBufferParams` from `ipc/message-exchange.js`
- Produces: `class CodeLldbBridge` (constructor `(dap: IDapClient)`) with `getBufferMetadata(variable: string, frameId: number): Promise<PlotBufferParams>`

- [ ] **Step 1: Create the fixture** — `0x42FF0010 = 1124007952` is a CV_8UC3 Mat (magic high bits + type bits `16` → channels 3, depth 0).

```json
// test/fixtures/cv-mat-variables.json
{
  "TestField.flags": "1124007952",
  "TestField.cols": "4",
  "TestField.rows": "2",
  "TestField.step.buf[0]": "12",
  "TestField.data": "0x1000",
  "memory": {
    "0x1000": [
      255, 0, 0, 0, 255, 0, 0, 0, 255, 10, 20, 30,
      40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150
    ]
  }
}
```

- [ ] **Step 2: Write the failing test**

```typescript
// test/codelldb-bridge.test.ts
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { CodeLldbBridge } from '../src/debugger/codelldb-bridge.js';
import type { IDapClient, EvaluateResult } from '../src/debugger/dap-client.js';

const fixture = JSON.parse(
  readFileSync(fileURLToPath(new URL('./fixtures/cv-mat-variables.json', import.meta.url)), 'utf8')
);

function fakeDap(): IDapClient {
  return {
    async evaluate(expression: string): Promise<EvaluateResult> {
      const value = fixture[expression];
      if (value === undefined) throw new Error(`no fixture for ${expression}`);
      const result: EvaluateResult = { result: value, variablesReference: 0 };
      if (expression.endsWith('.data')) result.memoryReference = value;
      return result;
    },
    async readMemory(memoryReference: string, count: number): Promise<Uint8Array> {
      return new Uint8Array((fixture.memory[memoryReference] as number[]).slice(0, count));
    },
    async topFrameId(): Promise<number> {
      return 0;
    },
  };
}

test('getBufferMetadata assembles PlotBufferParams from a cv::Mat', async () => {
  const bridge = new CodeLldbBridge(fakeDap());
  const params = await bridge.getBufferMetadata('TestField', 0);
  assert.equal(params.variableName, 'TestField');
  assert.equal(params.width, 4);
  assert.equal(params.height, 2);
  assert.equal(params.channels, 3);
  assert.equal(params.bufferType, 0);
  assert.equal(params.stride, 4);
  assert.equal(params.pixelLayout, 'bgra');
  assert.equal(params.transpose, false);
  assert.equal(params.pixels.length, 24); // rows(2) * step0(12)
  assert.equal(params.pixels[0], 255);
});

test('getBufferMetadata throws when the data pointer is unresolved', async () => {
  const dap = fakeDap();
  dap.evaluate = async (expr: string): Promise<EvaluateResult> =>
    expr.endsWith('.data')
      ? { result: '0x0', variablesReference: 0 } // no memoryReference
      : { result: fixture[expr], variablesReference: 0 };
  const bridge = new CodeLldbBridge(dap);
  await assert.rejects(bridge.getBufferMetadata('TestField', 0), /data pointer/);
});
```

- [ ] **Step 3: Run test to verify it fails**

Run: `npm run compile`
Expected: FAIL — `Cannot find module '../src/debugger/codelldb-bridge.js'`

- [ ] **Step 4: Write minimal implementation**

```typescript
// src/debugger/codelldb-bridge.ts
import { decodeMat } from '../typebridge/opencv-mat.js';
import { parseIntResult, type IDapClient } from './dap-client.js';
import type { PlotBufferParams } from '../ipc/message-exchange.js';

export class CodeLldbBridge {
  constructor(private readonly dap: IDapClient) {}

  async getBufferMetadata(variable: string, frameId: number): Promise<PlotBufferParams> {
    const field = async (expr: string): Promise<number> =>
      parseIntResult((await this.dap.evaluate(`${variable}.${expr}`, frameId)).result);

    const flags = await field('flags');
    const cols = await field('cols');
    const rows = await field('rows');
    const step0 = await field('step.buf[0]');

    const dataEval = await this.dap.evaluate(`${variable}.data`, frameId);
    if (!dataEval.memoryReference) {
      throw new Error(`Cannot resolve data pointer for '${variable}'`);
    }

    const meta = decodeMat({ flags, cols, rows, step0 });
    const pixels = await this.dap.readMemory(dataEval.memoryReference, meta.byteCount);

    return {
      variableName: variable,
      displayName: variable,
      pixelLayout: meta.pixelLayout,
      transpose: false,
      width: meta.width,
      height: meta.height,
      channels: meta.channels,
      stride: meta.stride,
      bufferType: meta.bufferType,
      pixels,
    };
  }
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `npm run compile && npm test`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/debugger/codelldb-bridge.ts test/codelldb-bridge.test.ts test/fixtures/cv-mat-variables.json
git commit -m "feat(debugger): assemble PlotBufferParams from a cv::Mat over DAP"
```

---

### Task 5: Session manager (watch loop)

**Files:**
- Create: `src/session/session-manager.ts`
- Test: `test/session-manager.test.ts`

**Interfaces:**
- Consumes: `PlotBufferParams` from `ipc/message-exchange.js`
- Produces:
  - `interface MetadataSource { getBufferMetadata(variable: string, frameId: number): Promise<PlotBufferParams> }`
  - `interface PlotSink { ensureReady(): Promise<void>; sendPlotBuffer(bytes: Uint8Array): void | Promise<void> }`
  - `interface SessionDeps { source: MetadataSource; sink: PlotSink; encode: (p: PlotBufferParams) => Uint8Array; resolveFrame: (threadId: number) => Promise<number>; getWatchList: () => string[]; warn: (message: string) => void }`
  - `class SessionManager` with `plotVariable(name: string, threadId: number): Promise<void>` and `onStopped(threadId: number): Promise<void>`

- [ ] **Step 1: Write the failing test**

```typescript
// test/session-manager.test.ts
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import { SessionManager, type SessionDeps } from '../src/session/session-manager.js';
import type { PlotBufferParams } from '../src/ipc/message-exchange.js';

function params(name: string): PlotBufferParams {
  return {
    variableName: name, displayName: name, pixelLayout: 'rgba', transpose: false,
    width: 1, height: 1, channels: 1, stride: 1, bufferType: 0, pixels: new Uint8Array([1]),
  };
}

function harness(overrides: Partial<SessionDeps> = {}) {
  const sent: string[] = [];
  const warnings: string[] = [];
  const deps: SessionDeps = {
    source: { async getBufferMetadata(name) {
      if (name === 'bad') throw new Error('not a Mat');
      return params(name);
    } },
    sink: { async ensureReady() {}, sendPlotBuffer(bytes) { sent.push(`#${bytes.length}`); } },
    encode: (p) => new Uint8Array([p.variableName.length]),
    resolveFrame: async () => 0,
    getWatchList: () => ['img', 'bad', 'img2'],
    warn: (m) => warnings.push(m),
    ...overrides,
  };
  return { mgr: new SessionManager(deps), sent, warnings };
}

test('onStopped plots every watch entry and isolates failures', async () => {
  const { mgr, sent, warnings } = harness();
  await mgr.onStopped(1);
  assert.equal(sent.length, 2);          // img + img2 sent; bad skipped
  assert.equal(warnings.length, 1);
  assert.match(warnings[0], /bad/);
});

test('plotVariable sends a single buffer', async () => {
  const { mgr, sent } = harness({ getWatchList: () => [] });
  await mgr.plotVariable('img', 1);
  assert.equal(sent.length, 1);
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `npm run compile`
Expected: FAIL — `Cannot find module '../src/session/session-manager.js'`

- [ ] **Step 3: Write minimal implementation**

```typescript
// src/session/session-manager.ts
import type { PlotBufferParams } from '../ipc/message-exchange.js';

export interface MetadataSource {
  getBufferMetadata(variable: string, frameId: number): Promise<PlotBufferParams>;
}

export interface PlotSink {
  ensureReady(): Promise<void>;
  sendPlotBuffer(bytes: Uint8Array): void | Promise<void>;
}

export interface SessionDeps {
  source: MetadataSource;
  sink: PlotSink;
  encode: (p: PlotBufferParams) => Uint8Array;
  resolveFrame: (threadId: number) => Promise<number>;
  getWatchList: () => string[];
  warn: (message: string) => void;
}

export class SessionManager {
  constructor(private readonly deps: SessionDeps) {}

  async plotVariable(name: string, threadId: number): Promise<void> {
    const frameId = await this.deps.resolveFrame(threadId);
    await this.plotAtFrame(name, frameId);
  }

  async onStopped(threadId: number): Promise<void> {
    const watch = this.deps.getWatchList();
    if (watch.length === 0) return;
    const frameId = await this.deps.resolveFrame(threadId);
    for (const name of watch) {
      await this.plotAtFrame(name, frameId);
    }
  }

  private async plotAtFrame(name: string, frameId: number): Promise<void> {
    try {
      const p = await this.deps.source.getBufferMetadata(name, frameId);
      await this.deps.sink.ensureReady();
      await this.deps.sink.sendPlotBuffer(this.deps.encode(p));
    } catch (err) {
      this.deps.warn(
        `OID: could not plot '${name}': ${err instanceof Error ? err.message : String(err)}`
      );
    }
  }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `npm run compile && npm test`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/session/session-manager.ts test/session-manager.test.ts
git commit -m "feat(session): watch-loop session manager with isolated failures"
```

---

### Task 6: ViewerController (panel refactor)

**Files:**
- Modify: `src/webview/panel.ts`

**Interfaces:**
- Produces:
  - `class ViewerController` with `open(): void`, `ensureReady(): Promise<void>`, `sendPlotBuffer(bytes: Uint8Array): void`
  - `function openViewerPanel(context: vscode.ExtensionContext): ViewerController` (back-compat for `oid.openPanel`)
- Satisfies the `PlotSink` shape from `session/session-manager.js` (`ensureReady` + `sendPlotBuffer`).

Rewrites `panel.ts` into a singleton controller. The webview HTML keeps the existing IPC glue (`toUint8Array`, the `oid-ipc-forward` handler, the `oid-viewer-ready` relay). The embedded smoke-test buffer is removed — buffers now arrive from the extension via `oid-ipc-forward`. `ensureReady()` resolves on the `viewer-ready` control message; sends queue until ready.

- [ ] **Step 1: Replace `src/webview/panel.ts` with the controller**

```typescript
// src/webview/panel.ts
import * as vscode from 'vscode';

function htmlFor(webview: vscode.Webview, media: vscode.Uri): string {
  const uri = (file: string) => webview.asWebviewUri(vscode.Uri.joinPath(media, file));
  const oidwindowJs = uri('oidwindow.js');
  const qtloaderJs = uri('qtloader.js');
  const oidwindowWasm = uri('oidwindow.wasm');

  return `<!DOCTYPE html>
<html lang="en-us">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, height=device-height, user-scalable=0">
  <title>Open Image Debugger</title>
  <style>
    html, body { padding: 0; margin: 0; overflow: hidden; width: 100%; height: 100%; }
    #screen { position: absolute; inset: 0; width: 100%; height: 100%; }
    #qtspinner { margin: 0; }
  </style>
</head>
<body onload="init()">
  <figure style="overflow:visible;" id="qtspinner">
    <center style="margin-top:1.5em; line-height:150%">
      <strong>Open Image Debugger</strong>
      <div id="qtstatus">Loading...</div>
    </center>
  </figure>
  <div id="screen"></div>

  <script>
    const vscode = acquireVsCodeApi();

    function toUint8Array(payload) {
      if (payload instanceof Uint8Array) return payload;
      if (Array.isArray(payload)) return new Uint8Array(payload);
      if (payload instanceof ArrayBuffer) return new Uint8Array(payload);
      if (payload && typeof payload === 'object') return new Uint8Array(Object.values(payload));
      throw new Error('invalid oid-ipc payload');
    }

    window.oidSend = function(u8) {
      const arr = u8 instanceof Uint8Array ? u8 : new Uint8Array(u8);
      vscode.postMessage({ type: 'oid-ipc-out', payload: Array.from(arr) });
    };

    function handleViewerReady(msg) { vscode.postMessage(msg); }
    window.oidOnViewerReady = handleViewerReady;
    window.addEventListener('oid-viewer-ready', (ev) => handleViewerReady(ev.detail));

    window.addEventListener('message', (ev) => {
      if (ev.data?.type !== 'oid-ipc-forward' || !ev.data.payload) return;
      const arr = toUint8Array(ev.data.payload);
      if (window.oidOnMessage) window.oidOnMessage(arr);
    });
  </script>

  <script type="text/javascript">
    async function init() {
      const spinner = document.querySelector('#qtspinner');
      const screen = document.querySelector('#screen');
      const status = document.querySelector('#qtstatus');
      const showUi = (ui) => {
        [spinner, screen].forEach((el) => el.style.display = 'none');
        if (screen === ui) screen.style.position = 'default';
        ui.style.display = 'block';
      };
      try {
        showUi(spinner);
        status.innerHTML = 'Loading...';
        const wasmResponse = await fetch('${oidwindowWasm}');
        if (!wasmResponse.ok) throw new Error('Failed to fetch oidwindow.wasm: ' + wasmResponse.status);
        const wasmModule = await WebAssembly.compile(await wasmResponse.arrayBuffer());
        await qtLoad({
          locateFile: (path) => (path === 'oidwindow.wasm' ? '${oidwindowWasm}' : path),
          qt: {
            module: wasmModule,
            onLoaded: () => showUi(screen),
            onExit: (exitData) => {
              status.innerHTML = 'Application exit';
              if (exitData.code !== undefined) status.innerHTML += ' with code ' + exitData.code;
              if (exitData.text !== undefined) status.innerHTML += ' (' + exitData.text + ')';
              showUi(spinner);
            },
            entryFunction: window.oidwindow_entry,
            containerElements: [screen],
          },
        });
      } catch (e) {
        console.error(e);
        status.innerHTML = String(e);
        showUi(spinner);
      }
    }
  </script>
  <script src="${oidwindowJs}"></script>
  <script src="${qtloaderJs}"></script>
</body>
</html>`;
}

/** Owns the single viewer webview and forwards encoded PlotBufferContents frames. */
export class ViewerController {
  private panel: vscode.WebviewPanel | undefined;
  private ready = false;
  private readyWaiters: Array<() => void> = [];
  private readonly media: vscode.Uri;

  constructor(private readonly context: vscode.ExtensionContext) {
    this.media = vscode.Uri.joinPath(context.extensionUri, 'media');
  }

  open(): void {
    if (this.panel) {
      this.panel.reveal(vscode.ViewColumn.One);
      return;
    }
    const panel = vscode.window.createWebviewPanel(
      'oidViewer',
      'Open Image Debugger',
      vscode.ViewColumn.One,
      { enableScripts: true, retainContextWhenHidden: true, localResourceRoots: [this.media] }
    );
    panel.webview.html = htmlFor(panel.webview, this.media);
    panel.webview.onDidReceiveMessage((msg) => {
      if (msg?.type === 'oid-control' && msg.event === 'viewer-ready') {
        this.ready = true;
        for (const w of this.readyWaiters.splice(0)) w();
      }
    });
    panel.onDidDispose(() => {
      this.panel = undefined;
      this.ready = false;
      this.readyWaiters = [];
    });
    this.panel = panel;
  }

  ensureReady(): Promise<void> {
    this.open();
    if (this.ready) return Promise.resolve();
    return new Promise<void>((resolve) => this.readyWaiters.push(resolve));
  }

  sendPlotBuffer(bytes: Uint8Array): void {
    this.panel?.webview.postMessage({ type: 'oid-ipc-forward', payload: Array.from(bytes) });
  }
}

/** Back-compat entry point for the `oid.openPanel` command. */
export function openViewerPanel(context: vscode.ExtensionContext): ViewerController {
  const controller = new ViewerController(context);
  controller.open();
  return controller;
}
```

- [ ] **Step 2: Compile to verify it type-checks**

Run: `npm run compile && npm test`
Expected: PASS (no type errors; existing unit tests still green)

- [ ] **Step 3: Manual smoke check** — `F5` → Extension Dev Host → run **OID: Open Viewer Panel**. The Qt canvas loads and shows the empty viewer (no auto image now — the smoke-test buffer was removed). Confirm no console errors.

- [ ] **Step 4: Commit**

```bash
git add src/webview/panel.ts
git commit -m "refactor(webview): ViewerController singleton, drop embedded smoke-test"
```

---

### Task 7: Extension wiring + manifest

**Files:**
- Modify: `src/extension.ts`
- Modify: `package.json`

**Interfaces:**
- Consumes: `ViewerController` from `webview/panel.js`; `DapClient` from `debugger/dap-client.js`; `CodeLldbBridge` from `debugger/codelldb-bridge.js`; `SessionManager` from `session/session-manager.js`; `buildPlotBufferContents` from `ipc/message-exchange.js`

- [ ] **Step 1: Update `package.json` (commands, activation, config)**

```json
{
  "name": "openimagedebugger-vscode",
  "displayName": "Open Image Debugger",
  "version": "0.0.1",
  "engines": { "vscode": "^1.85.0" },
  "activationEvents": ["onDebug"],
  "main": "./out/src/extension.js",
  "contributes": {
    "commands": [
      { "command": "oid.openPanel", "title": "OID: Open Viewer Panel" },
      { "command": "oid.plot", "title": "OID: Plot Variable" }
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
  "scripts": {
    "compile": "tsc -p .",
    "test": "node --test out/test/*.js"
  },
  "devDependencies": {
    "@types/vscode": "^1.85.0",
    "@types/node": "^20.0.0",
    "typescript": "^5.4.0"
  }
}
```

- [ ] **Step 2: Rewrite `src/extension.ts`**

```typescript
// src/extension.ts
import * as vscode from 'vscode';
import { ViewerController } from './webview/panel.js';
import { DapClient } from './debugger/dap-client.js';
import { CodeLldbBridge } from './debugger/codelldb-bridge.js';
import { SessionManager } from './session/session-manager.js';
import { buildPlotBufferContents } from './ipc/message-exchange.js';

function watchList(): string[] {
  return vscode.workspace.getConfiguration('oid').get<string[]>('watchOnStop', []);
}

function activeSession(): vscode.DebugSession | undefined {
  return vscode.debug.activeDebugSession;
}

export function activate(context: vscode.ExtensionContext): void {
  const viewer = new ViewerController(context);
  let stoppedThreadId: number | undefined;

  const manager = new SessionManager({
    source: {
      async getBufferMetadata(name, frameId) {
        const session = activeSession();
        if (!session) throw new Error('no active debug session');
        return new CodeLldbBridge(new DapClient(session)).getBufferMetadata(name, frameId);
      },
    },
    sink: viewer,
    encode: (p) => buildPlotBufferContents(p, 'wasm32'),
    resolveFrame: async (threadId) => {
      const session = activeSession();
      if (!session) throw new Error('no active debug session');
      return new DapClient(session).topFrameId(threadId);
    },
    getWatchList: watchList,
    warn: (m) => vscode.window.showWarningMessage(m),
  });

  // Track CodeLLDB stop events: capture the stopped thread and auto-plot the watch list.
  context.subscriptions.push(
    vscode.debug.registerDebugAdapterTrackerFactory('lldb', {
      createDebugAdapterTracker() {
        return {
          onDidSendMessage(message: any) {
            if (message.type === 'event' && message.event === 'stopped') {
              stoppedThreadId = message.body?.threadId;
              if (stoppedThreadId !== undefined) void manager.onStopped(stoppedThreadId);
            }
          },
        };
      },
    })
  );

  context.subscriptions.push(
    vscode.commands.registerCommand('oid.openPanel', () => viewer.open()),
    vscode.commands.registerCommand('oid.plot', async () => {
      if (!activeSession() || stoppedThreadId === undefined) {
        vscode.window.showWarningMessage('OID: pause in a debug session first.');
        return;
      }
      const editor = vscode.window.activeTextEditor;
      const selected = editor?.document.getText(editor.selection).trim();
      const name = selected || (await vscode.window.showInputBox({ prompt: 'Variable to plot' }))?.trim();
      if (name) await manager.plotVariable(name, stoppedThreadId);
    })
  );
}

export function deactivate(): void {}
```

- [ ] **Step 3: Compile + test**

Run: `npm run compile && npm test`
Expected: PASS (all unit tests; extension wiring is type-checked)

- [ ] **Step 4: Commit**

```bash
git add src/extension.ts package.json
git commit -m "feat(extension): wire CodeLLDB stop -> plot + oid.plot command"
```

---

### Task 8: Manual verification (testbench)

**Files:** none (verification only)

The OID testbench `testbench/main.cpp` declares `Mat TestField;` and fills it via `ones<uint8_t>(W, H, C, TestField)` inside `nest()`. Use it as the live target.

- [ ] **Step 1: Build the testbench (OID repo)**

```bash
cmake -S testbench -B testbench/build && cmake --build testbench/build -j
```

- [ ] **Step 2: Configure a CodeLLDB launch** — in `openimagedebugger-vscode/.vscode/launch.json`, add a `lldb` launch for the built testbench binary (under `../OpenImageDebugger/testbench/build/`), with a breakpoint inside `nest()` after `ones<uint8_t>(W, H, C, TestField)`.

- [ ] **Step 3: Run** — `F5` (Extension Dev Host) → start the CodeLLDB launch → hit the breakpoint in `nest()`.

- [ ] **Step 4: Plot on demand** — run **OID: Open Viewer Panel**, then **OID: Plot Variable** with `TestField` (select the identifier or type it). Confirm the image renders in the viewer.

- [ ] **Step 5: Auto-plot on stop** — set `"oid.watchOnStop": ["TestField"]`, continue/step to the next stop, confirm `TestField` re-plots automatically.

- [ ] **Step 6: Float check** — repeat against a float32 Mat (e.g. a `doSimpleCalculation<float>` path) and confirm it renders.

- [ ] **Step 7: Commit any launch.json fixture used**

```bash
git add .vscode/launch.json
git commit -m "test: CodeLLDB launch config for testbench verification"
```

---

## Plan self-review

- **Spec coverage:** §3 components → Tasks 1 (`buffer-metadata`), 2 (`opencv-mat`), 3 (`dap-client`), 4 (`codelldb-bridge`), 5 (`session-manager`), 6 (`ViewerController`/panel), 7 (`extension.ts`/manifest). §4 extraction → Tasks 2 + 4. §5 flow → Tasks 5 + 7. §6 error handling → Task 4 (null pointer/evaluate), Task 5 (isolated watch failures/warn), Task 7 (no stopped frame). §7 testing → unit Tasks 1–5, manual Task 8.
- **Placeholder scan:** No TBD / "add error handling" / "similar to" steps; every code step shows full code. The fixture flags literal is concrete (`1124007952`).
- **Type consistency:** `PlotBufferParams` (existing), `MatFields`/`MatMetadata`, `IDapClient`/`EvaluateResult`/`DapSession`, `MetadataSource`/`PlotSink`/`SessionDeps`, `ViewerController` are referenced with identical names/signatures across producing and consuming tasks. `getBufferMetadata`, `ensureReady`, `sendPlotBuffer`, `onStopped`, `plotVariable`, `topFrameId`, `parseIntResult` are spelled consistently.
- **Scope:** Extension-only; eight tasks each ending in a testable deliverable.
