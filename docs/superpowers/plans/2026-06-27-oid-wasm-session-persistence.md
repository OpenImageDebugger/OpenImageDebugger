# OID WASM Session Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persist OID viewer session state across VS Code restarts with behavioral parity to desktop C++ `SettingsManager`, using extension-owned storage and IPC types 6–7.

**Architecture:** Extension owns canonical state in `globalState`/`workspaceState`. On `viewer-ready`, extension sends `ApplySessionState` (type 6) with a full JSON snapshot. WASM applies via `session_state_codec` → existing `SettingsApplier` slots. On UI/buffer changes, WASM sends debounced `SessionStateChanged` (type 7) partial JSON; extension deep-merges and runs the C++ buffer-merge algorithm before saving.

**Tech Stack:** TypeScript (`node:test`), VS Code Extension API, Qt 6 / C++17, `QJsonDocument`, Emscripten WASM, legacy IPC wire format (`wasm32` little-endian u32 length-prefixed strings).

**Spec:** `docs/superpowers/specs/2026-06-27-oid-wasm-session-persistence-design.md`

## Global Constraints

- **Repos:** OID C++/WASM in `/Users/bruno/ws/OpenImageDebugger`; extension in `/Users/bruno/ws/openimagedebugger-vscode`.
- **Wire format:** `wasm32` — message-type header + u32 length-prefixed UTF-8 strings (no NUL).
- **Message types:** `ApplySessionState = 6`, `SessionStateChanged = 7` (already in both `message_exchange.h` and `message-exchange.ts`).
- **Already exists (extension):** `src/session/persistence.ts` with `loadSession`, `saveSession`, `mergeSessionDelta`, `migrateWatchOnStop`, `pruneExpiredBuffers`, debounced save; basic tests in `test/persistence.test.ts`. **Not yet wired** to panel/extension; **missing** C++ buffer-merge port and IPC codecs.
- **Not in scope:** export, TypeBridge, shared desktop INI, `selectedBuffer` persistence, window geometry.
- **Extension tests:** `npm run compile && npm test` in `openimagedebugger-vscode`.
- **OID tests:** `cmake --build build --target test_session_state_codec && ctest -R SessionStateCodec`.

---

## File map

| File | Responsibility |
|------|----------------|
| `openimagedebugger-vscode/src/session/persistence.ts` | Load/save/merge; buffer merge algorithm |
| `openimagedebugger-vscode/src/ipc/message-exchange.ts` | Encode type 6; decode type 7 |
| `openimagedebugger-vscode/src/webview/panel.ts` | Send restore on ready; route type 7 to persistence |
| `openimagedebugger-vscode/src/extension.ts` | Lifecycle wiring |
| `OpenImageDebugger/src/ui/messaging/session_state_codec.{h,cpp}` | JSON ↔ `SettingsApplier` |
| `OpenImageDebugger/src/ui/messaging/message_handler.cpp` | Decode type 6 |
| `OpenImageDebugger/src/ui/main_window/main_window.cpp` | WASM persist → type 7; skip QSettings load |
| `OpenImageDebugger/tests/test_session_state_codec.cpp` | Codec unit tests |

---

### Task 1: Extension buffer merge + schema cleanup

**Files:**
- Modify: `openimagedebugger-vscode/src/session/persistence.ts`
- Modify: `openimagedebugger-vscode/test/persistence.test.ts`

**Interfaces produced:**
- `mergeBuffersFromWasmDelta(state, delta, now?): OidSessionState`
- `sessionStateForRestore(state, now?): OidSessionState` — prune expired before sending to WASM
- Remove `selectedBuffer` from `OidSessionState` and `SessionDelta`

- [ ] **Step 1: Write failing buffer-merge tests**

Add to `test/persistence.test.ts`:

```typescript
import { mergeBuffersFromWasmDelta, sessionStateForRestore } from '../src/session/persistence.js';

test('mergeBuffersFromWasmDelta keeps unheld unexpired buffers', () => {
  const now = 1_000_000;
  const state = mergeSessionDelta(defaultSessionState(), {
    buffers: {
      watched: [
        { name: 'idle', expiresAt: now + 5000 },
        { name: 'stale', expiresAt: now - 1 },
      ],
      removed: [],
    },
  });
  const merged = mergeBuffersFromWasmDelta(
    state,
    { held: ['active'], removed: [] },
    now
  );
  assert.deepEqual(
    merged.buffers.watched.map((w) => w.name).sort(),
    ['active', 'idle']
  );
  assert.equal(merged.buffers.watched.find((w) => w.name === 'active')!.expiresAt, now + BUFFER_EXPIRY_MS);
});

test('mergeBuffersFromWasmDelta drops removed and clears removed list', () => {
  const now = 2_000_000;
  const state = mergeSessionDelta(defaultSessionState(), {
    buffers: {
      watched: [{ name: 'gone', expiresAt: now + 5000 }],
      removed: [],
    },
  });
  const merged = mergeBuffersFromWasmDelta(state, { held: [], removed: ['gone'] }, now);
  assert.deepEqual(merged.buffers.watched, []);
  assert.deepEqual(merged.buffers.removed, []);
});

test('sessionStateForRestore filters expired watched entries', () => {
  const now = 3_000_000;
  const state = mergeSessionDelta(defaultSessionState(), {
    buffers: {
      watched: [
        { name: 'ok', expiresAt: now + 1 },
        { name: 'dead', expiresAt: now - 1 },
      ],
      removed: [],
    },
  });
  const restored = sessionStateForRestore(state, now);
  assert.deepEqual(
    restored.buffers.watched.map((w) => w.name),
    ['ok']
  );
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile && npm test`
Expected: FAIL — `mergeBuffersFromWasmDelta is not exported`

- [ ] **Step 3: Implement merge helpers and remove selectedBuffer**

Add to `persistence.ts`:

```typescript
export interface BufferPersistDelta {
  held: readonly string[];
  removed: readonly string[];
}

export function mergeBuffersFromWasmDelta(
  state: OidSessionState,
  delta: BufferPersistDelta,
  now = Date.now()
): OidSessionState {
  const nextExpiry = now + BUFFER_EXPIRY_MS;
  const heldSet = new Set(delta.held);
  const removedSet = new Set(delta.removed);
  const watched: WatchedBuffer[] = [];

  for (const entry of state.buffers.watched) {
    if (removedSet.has(entry.name)) continue;
    if (heldSet.has(entry.name)) continue;
    if (entry.expiresAt < now) continue;
    watched.push(entry);
  }
  for (const name of delta.held) {
    watched.push({ name, expiresAt: nextExpiry });
  }

  return mergeSessionDelta(state, { buffers: { watched, removed: [] } });
}

export function sessionStateForRestore(state: OidSessionState, now = Date.now()): OidSessionState {
  return pruneExpiredBuffers(state, now);
}

export function applySessionDelta(
  state: OidSessionState,
  partial: SessionDelta,
  now = Date.now()
): OidSessionState {
  const merged = mergeSessionDelta(state, partial);
  if (partial.buffers?.held || partial.buffers?.removed) {
    return mergeBuffersFromWasmDelta(merged, {
      held: partial.buffers.held ?? [],
      removed: partial.buffers.removed ?? merged.buffers.removed,
    }, now);
  }
  return merged;
}
```

Extend `SessionDelta` / WASM delta shape:

```typescript
export type SessionDelta = {
  rendering?: Partial<OidSessionState['rendering']>;
  export?: Partial<OidSessionState['export']>;
  ui?: Partial<OidSessionState['ui']>;
  buffers?: Partial<OidSessionState['buffers']> & {
    held?: readonly string[];
  };
};
```

Remove `selectedBuffer` from `OidSessionState`, `SessionDelta`, and `mergeSessionDelta`.

- [ ] **Step 4: Run tests**

Run: `npm run compile && npm test`
Expected: PASS

- [ ] **Step 5: Commit (extension repo)**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode
git add src/session/persistence.ts test/persistence.test.ts
git commit -m "feat(session): add buffer merge algorithm for WASM deltas"
```

---

### Task 2: Extension IPC codecs for types 6–7

**Files:**
- Modify: `openimagedebugger-vscode/src/ipc/message-exchange.ts`
- Create: `openimagedebugger-vscode/test/message-exchange-session.test.ts`

- [ ] **Step 1: Write failing IPC tests**

```typescript
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import {
  MessageType,
  buildApplySessionState,
  decodeInbound,
} from '../src/ipc/message-exchange.js';

test('buildApplySessionState encodes type 6 + JSON string', () => {
  const json = '{"version":1}';
  const bytes = buildApplySessionState(json);
  assert.equal(bytes[0], MessageType.ApplySessionState);
  const len = bytes[4] | (bytes[5] << 8) | (bytes[6] << 16) | (bytes[7] << 24);
  assert.equal(len, json.length);
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
Expected: FAIL — `buildApplySessionState is not exported`

- [ ] **Step 3: Implement codecs**

In `message-exchange.ts`:

```typescript
export function buildApplySessionState(json: string, mode: WasmLengthMode = 'wasm32'): Uint8Array {
  const parts: number[] = [];
  pushU32(parts, MessageType.ApplySessionState);
  pushString(parts, json, mode);
  return new Uint8Array(parts);
}

export type InboundMessage =
  | { kind: 'observedSymbols'; names: string[] }
  | { kind: 'plotRequest'; name: string }
  | { kind: 'exportRequest'; name: string; format: ExportFormat; contrast: Float32Array }
  | { kind: 'sessionStateChanged'; json: string }
  | { kind: 'unknown'; type: number };
```

Add to `decodeInbound` switch:

```typescript
case MessageType.SessionStateChanged:
  return { kind: 'sessionStateChanged', json: readString() };
```

- [ ] **Step 4: Run tests**

Run: `npm run compile && npm test`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/ipc/message-exchange.ts test/message-exchange-session.test.ts
git commit -m "feat(ipc): add ApplySessionState and SessionStateChanged codecs"
```

---

### Task 3: Extension lifecycle wiring

**Files:**
- Modify: `openimagedebugger-vscode/src/webview/panel.ts`
- Modify: `openimagedebugger-vscode/src/extension.ts`

- [ ] **Step 1: Extend ViewerController with session hooks**

In `panel.ts`, add imports and fields:

```typescript
import { buildApplySessionState } from '../ipc/message-exchange.js';
import type { SessionDelta } from '../session/persistence.js';

// in ViewerController:
private onSessionStateChanged: ((json: string) => void) | undefined;
private sessionRestoreJson: string | undefined;
private restoreSent = false;

setSessionStateChangedHandler(handler: (json: string) => void): void {
  this.onSessionStateChanged = handler;
}

queueSessionRestore(json: string): void {
  this.sessionRestoreJson = json;
  this.restoreSent = false;
  void this.maybeSendSessionRestore();
}

private async maybeSendSessionRestore(): Promise<void> {
  if (!this.ready || this.restoreSent || !this.sessionRestoreJson) return;
  await this.sendFrame(buildApplySessionState(this.sessionRestoreJson));
  this.restoreSent = true;
}
```

In `onDidReceiveMessage`, after setting `ready = true`:

```typescript
void this.maybeSendSessionRestore();
```

And in IPC decode branch:

```typescript
} else if (decoded.kind === 'sessionStateChanged') {
  this.onSessionStateChanged?.(decoded.json);
}
```

- [ ] **Step 2: Wire extension.ts**

```typescript
import {
  loadSession,
  migrateWatchOnStop,
  sessionStateForRestore,
  encodeSessionState,
  applySessionDelta,
  saveSessionDebounced,
  type OidSessionState,
} from './session/persistence.js';

let sessionState: OidSessionState = loadSession(context);
let watchSeededThisDebugSession = false;

viewer.setSessionStateChangedHandler((json) => {
  try {
    const partial = JSON.parse(json) as SessionDelta;
    sessionState = applySessionDelta(sessionState, partial);
    saveSessionDebounced(context, sessionState);
  } catch (err) {
    console.warn('OID: invalid SessionStateChanged JSON', err);
  }
});

// inside viewer-ready path — add helper called when panel opens first time per stop:
function prepareSessionRestore(): void {
  if (!watchSeededThisDebugSession) {
    sessionState = migrateWatchOnStop(sessionState, watchList());
    watchSeededThisDebugSession = true;
  }
  const restore = sessionStateForRestore(sessionState);
  viewer.queueSessionRestore(encodeSessionState(restore));
}

// Call prepareSessionRestore() when viewer.openBeside() first runs per debug session
// (e.g. in onStopped before manager.onStopped, after ensureReady is unnecessary — queue handles ready gate)
```

Reset `watchSeededThisDebugSession = false` in `onDidTerminateDebugSession`.

Reload `sessionState = loadSession(context)` on extension activate only (already at init); do not reload on every stop.

- [ ] **Step 3: Compile**

Run: `npm run compile`
Expected: no errors

- [ ] **Step 4: Commit**

```bash
git add src/webview/panel.ts src/extension.ts
git commit -m "feat(session): restore and persist session state over IPC"
```

---

### Task 4: C++ session_state_codec

**Files:**
- Create: `OpenImageDebugger/src/ui/messaging/session_state_codec.h`
- Create: `OpenImageDebugger/src/ui/messaging/session_state_codec.cpp`
- Create: `OpenImageDebugger/tests/test_session_state_codec.cpp`
- Modify: `OpenImageDebugger/src/CMakeLists.txt`
- Modify: `OpenImageDebugger/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing codec test**

Create `tests/test_session_state_codec.cpp` with a mock/minimal applier recording calls, or test against real `SettingsApplier` with stub deps. Minimal approach — test JSON parsing helpers:

```cpp
#include <gtest/gtest.h>
#include <QJsonDocument>
#include "ui/messaging/session_state_codec.h"

TEST(SessionStateCodec, ParsesFramerateAndContrast) {
  const auto json = R"({"rendering":{"framerate":30},"ui":{"contrastEnabled":true}})";
  SessionStateFields fields;
  ASSERT_TRUE(parse_session_state_json(json, fields));
  EXPECT_DOUBLE_EQ(fields.framerate.value(), 30.0);
  EXPECT_TRUE(fields.contrast_enabled.value());
}

TEST(SessionStateCodec, ParsesWatchedBufferNames) {
  const auto json = R"({"buffers":{"watched":[{"name":"img","expiresAt":9999999999000}]}})";
  SessionStateFields fields;
  ASSERT_TRUE(parse_session_state_json(json, fields));
  ASSERT_EQ(fields.previous_session_buffers.size(), 1);
  EXPECT_EQ(fields.previous_session_buffers.front(), QStringLiteral("img"));
}
```

- [ ] **Step 2: Run test — expect FAIL**

Run: `cmake --build build --target test_session_state_codec && ctest -R SessionStateCodec -V`
Expected: build failure — target missing

- [ ] **Step 3: Implement codec**

`session_state_codec.h`:

```cpp
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
  QStringList previous_session_buffers;
};

bool parse_session_state_json(QByteArray json, SessionStateFields& out);
void apply_session_state_fields(const SessionStateFields& fields, SettingsApplier& applier);
QByteArray serialize_session_state_delta(/* callback sources matching MainWindow::persist_settings */);
```

Implement `apply_session_state_fields` by calling existing `SettingsApplier` slots (mirror `SettingsManager::load_*` mapping). Colorspace: parse 4-char string using same `b/g/r/a` → channel name logic as `settings_manager.cpp`.

- [ ] **Step 4: Add to CMake**

Add `session_state_codec.cpp` to `src/CMakeLists.txt` SOURCES.

Add `test_session_state_codec` executable in `tests/CMakeLists.txt` (link Qt6::Core, gtest, oid sources or codec-only).

- [ ] **Step 5: Run tests**

Expected: PASS

- [ ] **Step 6: Commit (OID repo)**

```bash
cd /Users/bruno/ws/OpenImageDebugger
git add src/ui/messaging/session_state_codec.* tests/test_session_state_codec.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(session): add session_state_codec for ApplySessionState JSON"
```

---

### Task 5: C++ MessageHandler decode ApplySessionState

**Files:**
- Modify: `OpenImageDebugger/src/ui/messaging/message_handler.h`
- Modify: `OpenImageDebugger/src/ui/messaging/message_handler.cpp`
- Modify: `OpenImageDebugger/src/ui/main_window/main_window.cpp`

- [ ] **Step 1: Add SettingsApplier to MessageHandler deps (EMSCRIPTEN only or always)**

```cpp
// message_handler.h Dependencies struct:
SettingsApplier* settings_applier = nullptr; // non-owning; set on WASM path
```

- [ ] **Step 2: Implement decode_apply_session_state**

```cpp
void MessageHandler::decode_apply_session_state() {
  auto message_decoder = MessageDecoder{deps_.transport};
  auto json = std::string{};
  message_decoder.read(json);
  if (deps_.settings_applier == nullptr) return;
  SessionStateFields fields;
  if (!parse_session_state_json(QByteArray::fromStdString(json), fields)) {
    std::cerr << "[OID] Invalid ApplySessionState JSON\n";
    return;
  }
  apply_session_state_fields(fields, *deps_.settings_applier);
}
```

Add `case MessageType::ApplySessionState:` to `decode_incoming_messages()`.

Pass `settings_applier_.get()` when constructing `MessageHandler` in `main_window.cpp`.

- [ ] **Step 3: Guard QSettings load on WASM**

```cpp
#ifndef __EMSCRIPTEN__
    settings_manager_->load_settings();
#endif
```

- [ ] **Step 4: Build WASM target**

Run: `cmake --build build-wasm` (or project's emscripten build command)
Expected: compiles

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(session): decode ApplySessionState in MessageHandler"
```

---

### Task 6: C++ WASM persist → SessionStateChanged

**Files:**
- Modify: `OpenImageDebugger/src/ui/main_window/main_window.cpp`
- Modify: `OpenImageDebugger/src/ui/messaging/session_state_codec.cpp`

- [ ] **Step 1: Implement serialize_session_state_delta**

Build JSON object from the same `DataCallbacks` already populated in `MainWindow::persist_settings()`:

```cpp
#ifdef __EMSCRIPTEN__
void MainWindow::persist_settings() {
  SettingsManager::DataCallbacks callbacks;
  // ... existing callback lambdas unchanged ...

  const auto json = serialize_session_state_delta(callbacks);
  auto composer = MessageComposer{};
  composer.push(MessageType::SessionStateChanged).push(std::string(json.constData(), json.size()))
      .send(*postmessage_transport_);
  return;
}
#endif
SettingsManager::persist_settings(callbacks); // desktop path
```

JSON shape must match extension `SessionDelta`:

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
    "minmaxCompact": false,
    "colorspace": "rgba"
  },
  "buffers": {
    "held": ["img"],
    "removed": ["old"]
  }
}
```

Collect `held` from `getCurrentBufferNames()`, `removed` from `getRemovedBufferNames()`, then call `clearRemovedBufferNames()` after serialize (same order as desktop persist).

Also wire persist triggers for fields desktop doesn't write today but spec requires: list position, minmax compact, colorspace — read from UI state in callbacks.

- [ ] **Step 2: Add codec round-trip test for serialize**

Extend `test_session_state_codec.cpp` verifying serialize output contains `"held"` array keys.

- [ ] **Step 3: Rebuild WASM and copy to extension media**

Run project's wasm pack script (e.g. copy `oidwindow.wasm` / `oidwindow.js` to `openimagedebugger-vscode/media/`).

- [ ] **Step 4: Commit both repos**

```bash
# OID
git commit -m "feat(session): emit SessionStateChanged JSON from WASM persist"

# Extension (media update)
git add media/oidwindow.wasm media/oidwindow.js
git commit -m "chore(media): rebuild WASM with session persistence IPC"
```

---

### Task 7: Manual verification

- [ ] **Step 1: Toggle contrast + move splitter → reload VS Code window → verify restored**

- [ ] **Step 2: Add buffer to watch list → reload VS Code → break on next debug stop → verify auto-plot when symbol in scope**

- [ ] **Step 3: Remove buffer → reload → verify it does not return**

- [ ] **Step 4: Confirm desktop OID QSettings file unchanged after WASM session**

---

## Self-review

| Spec requirement | Task |
|------------------|------|
| Extension-owned storage | 1, 3 |
| IPC types 6–7 | 2, 5, 6 |
| C++ SettingsApplier apply path | 4, 5 |
| C++ persist callbacks + 100 ms debounce | 6 (WASM emit; extension debounce in Task 1) |
| Buffer merge algorithm | 1 |
| watchOnStop migration | 3 |
| Skip QSettings on WASM | 5 |
| No window geometry / selectedBuffer | 1, 4 |
| Persist listPosition/minmaxCompact/colorspace | 6 |
| Error handling invalid JSON | 3, 5 |
| Unit tests | 1, 2, 4 |

**Placeholder scan:** No TBD steps. All function names consistent (`mergeBuffersFromWasmDelta`, `buildApplySessionState`, `parse_session_state_json`, `apply_session_state_fields`).

---

## Execution handoff

Plan complete and saved to `docs/superpowers/plans/2026-06-27-oid-wasm-session-persistence.md`. Two execution options:

**1. Subagent-Driven (recommended)** — fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** — execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
