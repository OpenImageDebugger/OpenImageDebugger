# OID VS Code P3 Refine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete viewer-owned P3 — WASM autocomplete, viewer-initiated plot requests, and `GetObservedSymbols` poll on stop — replacing the interim extension-owned `liveNames` model.

**Architecture:** Fix `MessageComposer` coalescing in OID so viewer→host IPC is one postMessage frame; rebuild WASM into extension `media/`. Extend the extension with symbol enumeration, `ViewerChannel` inbound handling, and the P3 stop loop. Remove `oid.clearWatch`.

**Tech Stack:** C++ (MessageComposer), Emscripten/Qt WASM, TypeScript VS Code extension, DAP scopes/variables, `node:test`.

**Spec:** `docs/superpowers/specs/2026-06-27-oid-vscode-p3-refine-design.md`

**Repos:** `OpenImageDebugger` + `openimagedebugger-vscode`

---

## Already done (skip)

- `message-exchange.ts` encoders/decoders for types 0,1,2,4 + unit tests
- `buildPlotBufferContents`, CodeLLDB bridge, editor-split `ViewerController`
- Live-watch `liveNames` (to be **removed** in Task 6)

---

## Task 1: Coalesce `MessageComposer::send(ITransport&)`

**Files:**
- Modify: `OpenImageDebugger/src/ipc/message_exchange.h`
- Test: `OpenImageDebugger/tests/test_message_exchange.cpp`

- [ ] **Step 1: Add recording mock transport in test file**

At top of test cpp (inside `oid` namespace or test fixture), add:

```cpp
struct RecordingTransport final : oid::ITransport {
  std::vector<std::vector<std::byte>> sends;
  void send(std::span<const std::byte> data) override {
    sends.emplace_back(data.begin(), data.end());
  }
  std::size_t receive(std::span<std::byte>) override { return 0; }
  bool has_data() const override { return false; }
};
```

- [ ] **Step 2: Write failing test**

```cpp
TEST_F(MessageExchangeTest, MessageComposerSendCoalescesBlocks) {
  RecordingTransport transport;
  MessageComposer composer;
  composer.push(MessageType::PlotBufferRequest).push(std::string("TestField"));
  composer.send(transport);
  ASSERT_EQ(transport.sends.size(), 1u);
  ASSERT_GE(transport.sends[0].size(), 12u); // type + len + at least some string bytes
  const auto type = std::bit_cast<MessageType>(transport.sends[0].data());
  EXPECT_EQ(type, MessageType::PlotBufferRequest);
}
```

- [ ] **Step 3: Implement coalescing in `message_exchange.h`**

Replace `send(ITransport&)` body:

```cpp
void send(ITransport& transport) const {
    std::size_t total = 0;
    for (const auto& block : message_blocks_) {
        total += block->size();
    }
    std::vector<std::byte> frame;
    frame.reserve(total);
    for (const auto& block : message_blocks_) {
        const auto* data = block->data();
        frame.insert(frame.end(), data, data + block->size());
    }
    if (!frame.empty()) {
        transport.send(frame);
    }
}
```

Add `#include <vector>` if not already present via other headers.

- [ ] **Step 4: Run C++ tests**

Run: `cd OpenImageDebugger && cmake --build cmake-build-debug --target test_message_exchange && ctest -R MessageExchangeTest --test-dir cmake-build-debug`  
(Adjust build dir to your local preset if different.)

Expected: all `MessageExchangeTest` pass including new test.

- [ ] **Step 5: Commit (OpenImageDebugger)**

```bash
git add src/ipc/message_exchange.h tests/test_message_exchange.cpp
git commit -m "fix(ipc): coalesce MessageComposer blocks into one transport send"
```

---

## Task 2: Rebuild WASM and refresh extension media

**Files:**
- Copy: `build-wasm/oidwindow.wasm`, `oidwindow.js` → `openimagedebugger-vscode/media/`

- [ ] **Step 1: Rebuild oidwindow**

```bash
cd OpenImageDebugger
source /path/to/emsdk/emsdk_env.sh
/path/to/qt6-wasm/bin/qt-cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm --target oidwindow
```

- [ ] **Step 2: Copy artifacts**

```bash
cp build-wasm/oidwindow.wasm build-wasm/oidwindow.js ../openimagedebugger-vscode/media/
```

- [ ] **Step 3: Commit both repos**

OpenImageDebugger: only if source changed beyond Task 1 (usually Task 1 commit suffices).

```bash
cd openimagedebugger-vscode
git add media/oidwindow.wasm media/oidwindow.js
git commit -m "chore(media): refresh WASM after MessageComposer coalescing fix"
```

---

## Task 3: DAP symbol enumerator

**Files:**
- Modify: `openimagedebugger-vscode/src/debugger/dap-client.ts`
- Create: `openimagedebugger-vscode/src/debugger/symbol-enumerator.ts`
- Create: `openimagedebugger-vscode/test/symbol-enumerator.test.ts`

- [ ] **Step 1: Extend `IDapClient` and `DapClient`**

Add to `dap-client.ts`:

```typescript
export interface ScopeInfo {
  name: string;
  variablesReference: number;
}

export interface VariableInfo {
  name: string;
  type?: string;
}

// In IDapClient:
scopes(frameId: number): Promise<ScopeInfo[]>;
variables(variablesReference: number): Promise<VariableInfo[]>;

// In DapClient:
async scopes(frameId: number): Promise<ScopeInfo[]> {
  const res = await this.session.customRequest('scopes', { frameId });
  return (res.scopes ?? []).map((s: any) => ({
    name: s.name,
    variablesReference: s.variablesReference,
  }));
}

async variables(variablesReference: number): Promise<VariableInfo[]> {
  const res = await this.session.customRequest('variables', { variablesReference });
  return (res.variables ?? []).map((v: any) => ({ name: v.name, type: v.type }));
}
```

- [ ] **Step 2: Create `symbol-enumerator.ts`**

```typescript
import type { IDapClient, VariableInfo } from './dap-client.js';

const ALLOWED_SCOPES = new Set(['Local', 'Arguments']);

export async function listScopeSymbols(
  client: IDapClient,
  frameId: number
): Promise<VariableInfo[]> {
  const scopes = await client.scopes(frameId);
  const out: VariableInfo[] = [];
  for (const scope of scopes) {
    if (!ALLOWED_SCOPES.has(scope.name)) continue;
    if (scope.variablesReference <= 0) continue;
    const vars = await client.variables(scope.variablesReference);
    out.push(...vars);
  }
  return out;
}
```

- [ ] **Step 3: Write tests with mock client**

Test keeps Local+Arguments, drops Registers; merges variables from both scopes.

- [ ] **Step 4: `npm run compile && npm test`**

- [ ] **Step 5: Commit**

```bash
git add src/debugger/dap-client.ts src/debugger/symbol-enumerator.ts test/symbol-enumerator.test.ts
git commit -m "feat(debugger): enumerate Local and Arguments scope symbols"
```

---

## Task 4: ViewerChannel in `panel.ts`

**Files:**
- Modify: `openimagedebugger-vscode/src/webview/panel.ts`

- [ ] **Step 1: Add imports and constants**

```typescript
import {
  buildSetAvailableSymbols,
  buildGetObservedSymbols,
  decodeInbound,
} from '../ipc/message-exchange.js';

const OBSERVED_TIMEOUT_MS = 1_000;
```

- [ ] **Step 2: Add channel state to `ViewerController`**

Fields:

```typescript
private pendingObserved: ((names: string[]) => void) | undefined;
private onPlotBufferRequest: ((name: string) => void) | undefined;

setPlotBufferRequestHandler(handler: (name: string) => void): void {
  this.onPlotBufferRequest = handler;
}
```

- [ ] **Step 3: Handle `oid-ipc-out` in `onDidReceiveMessage`**

```typescript
if (msg?.type === 'oid-ipc-out' && msg.payload) {
  const bytes = new Uint8Array(msg.payload);
  const decoded = decodeInbound(bytes);
  if (decoded.kind === 'observedSymbols') {
    this.pendingObserved?.(decoded.names);
    this.pendingObserved = undefined;
  } else if (decoded.kind === 'plotRequest') {
    this.onPlotBufferRequest?.(decoded.name);
  }
  return;
}
```

- [ ] **Step 4: Add `sendFrame`, `setAvailableSymbols`, `getObservedSymbols`**

```typescript
private async sendFrame(bytes: Uint8Array): Promise<void> {
  await this.ensureReady();
  this.panel?.webview.postMessage({ type: 'oid-ipc-forward', payload: Array.from(bytes) });
}

async setAvailableSymbols(names: string[]): Promise<void> {
  await this.sendFrame(buildSetAvailableSymbols(names, 'wasm32'));
}

async getObservedSymbols(): Promise<string[]> {
  await this.sendFrame(buildGetObservedSymbols());
  return new Promise<string[]>((resolve) => {
    if (this.pendingObserved) this.pendingObserved([]);
    const timer = setTimeout(() => {
      this.pendingObserved = undefined;
      console.warn('OID: GetObservedSymbols timed out');
      resolve([]);
    }, OBSERVED_TIMEOUT_MS);
    this.pendingObserved = (names) => {
      clearTimeout(timer);
      resolve(names);
    };
  });
}
```

Wire `pendingObserved` resolver in the `oid-ipc-out` handler as shown.

- [ ] **Step 5: Commit**

```bash
git add src/webview/panel.ts
git commit -m "feat(webview): ViewerChannel for symbols and inbound plot requests"
```

---

## Task 5: P3 session loop (replace liveNames)

**Files:**
- Modify: `openimagedebugger-vscode/src/session/session-manager.ts`
- Modify: `openimagedebugger-vscode/test/session-manager.test.ts`

- [ ] **Step 1: Extend `SessionDeps`**

```typescript
export interface ViewerChannel {
  setAvailableSymbols(names: string[]): Promise<void>;
  getObservedSymbols(): Promise<string[]>;
}

export interface SessionDeps {
  source: MetadataSource;
  sink: PlotSink;
  channel: ViewerChannel;
  listScopeSymbols: (frameId: number) => Promise<{ name: string }[]>;
  encode: (p: PlotBufferParams) => Uint8Array;
  resolveFrame: (threadId: number) => Promise<number>;
  getWatchList: () => string[];
  warn: (message: string) => void;
}
```

Remove `liveNames` field. Add `private seeded = false`.

- [ ] **Step 2: Replace `onStopped` with P3 loop**

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
      this.deps.warn(`OID: could not list symbols: ${err instanceof Error ? err.message : String(err)}`);
    }
    let observed = await this.deps.channel.getObservedSymbols();
    if (!this.seeded) {
      observed = [...new Set([...observed, ...this.deps.getWatchList()])];
      this.seeded = true;
    }
    for (const name of observed) {
      await this.plotAtFrame(name, frameId, { warnOnError: true });
    }
  } catch {
    // frame resolution failure — silent
  } finally {
    this.replotting = false;
  }
}
```

- [ ] **Step 3: Add `onPlotBufferRequest`**

```typescript
async onPlotBufferRequest(name: string, threadId: number): Promise<void> {
  const frameId = await this.deps.resolveFrame(threadId);
  await this.plotAtFrame(name, frameId, { warnOnError: true });
}
```

- [ ] **Step 4: Change `plotVariable` to delegate**

```typescript
async plotVariable(name: string, threadId: number): Promise<void> {
  await this.onPlotBufferRequest(name, threadId);
}
```

- [ ] **Step 5: `reset()` clears `seeded` only**

```typescript
reset(): void {
  this.seeded = false;
}
```

- [ ] **Step 6: Rewrite session-manager tests**

Replace live-watch tests with mocks for `channel` + `listScopeSymbols`:

- `setAvailableSymbols` called with enumerated names on stop
- `getObservedSymbols` result plotted (one `sendPlotBuffer` per name)
- first stop unions watch list into observed set
- `onPlotBufferRequest` plots with warn on failure
- overlap guard retained
- `reset` clears seeded so next session re-seeds watch list

- [ ] **Step 7: `npm run compile && npm test`**

- [ ] **Step 8: Commit**

```bash
git add src/session/session-manager.ts test/session-manager.test.ts
git commit -m "feat(session): viewer-owned stop loop with symbol list and poll"
```

---

## Task 6: Wire `extension.ts` and remove `oid.clearWatch`

**Files:**
- Modify: `openimagedebugger-vscode/src/extension.ts`
- Modify: `openimagedebugger-vscode/package.json`

- [ ] **Step 1: Remove `oid.clearWatch` from `package.json` commands**

- [ ] **Step 2: Wire deps in `extension.ts`**

```typescript
import { listScopeSymbols } from './debugger/symbol-enumerator.js';

// In activate, after creating viewer:
viewer.setPlotBufferRequestHandler((name) => {
  if (stoppedThreadId === undefined) {
    vscode.window.showWarningMessage('OID: pause in a debug session first.');
    return;
  }
  void manager.onPlotBufferRequest(name, stoppedThreadId);
});

const manager = new SessionManager({
  source: { /* unchanged */ },
  sink: viewer,
  channel: viewer,
  listScopeSymbols: async (frameId) => {
    const session = activeSession();
    if (!session) throw new Error('no active debug session');
    return listScopeSymbols(new DapClient(session), frameId);
  },
  encode: (p) => buildPlotBufferContents(p, 'wasm32'),
  resolveFrame: async (threadId) => { /* unchanged */ },
  getWatchList: watchList,
  warn: (m) => vscode.window.showWarningMessage(m),
});
```

Remove `oid.clearWatch` command registration.

Make `ViewerController` implement `ViewerChannel` interface (TypeScript structural typing — methods already on class).

- [ ] **Step 3: `npm run compile && npm test`**

- [ ] **Step 4: Commit**

```bash
git add src/extension.ts package.json
git commit -m "feat(extension): wire P3 viewer channel and drop clearWatch command"
```

---

## Task 7: Manual verification

- [ ] Break in testbench — WASM autocomplete shows locals/arguments
- [ ] Add variable via viewer symbol box — plots immediately
- [ ] `oid.watchOnStop` seeds on first stop
- [ ] Step/continue refreshes observed buffers
- [ ] Remove buffer in viewer — not replotted on next stop
- [ ] Debug session end disposes panel; new session re-seeds watch list

---

## Plan self-review

| Spec § | Task |
|--------|------|
| MessageComposer coalescing | Task 1 |
| WASM refresh | Task 2 |
| Symbol enumeration | Task 3 |
| ViewerChannel inbound/outbound | Task 4 |
| P3 stop loop, remove liveNames | Task 5 |
| Wire extension, remove clearWatch | Task 6 |
| Manual P3 checklist | Task 7 |

No placeholders. `ViewerController` satisfies `ViewerChannel` structurally.
