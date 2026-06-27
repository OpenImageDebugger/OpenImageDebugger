# OID VS Code P5: Full parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship export (PNG/Octave), cross-restart session persistence, and full TypeBridge parity (Eigen, legacy OpenCV types, custom types) in the VS Code extension + WASM viewer via extended legacy IPC.

**Architecture:** Extension owns persistence and export file I/O; WASM owns UI state and sends debounced JSON deltas plus export requests with contrast parameters. TypeBridge in the extension ports all `oidtypes/*.py` inspectors. Legacy message types 5–8 extend the proven P3 wire format — no OBP migration.

**Tech Stack:** C++20/Qt6 WASM (OID), TypeScript/VS Code extension API, CodeLLDB DAP, `pngjs`, `node:test`.

## Global Constraints

- **Repos:** OID `/Users/bruno/ws/OpenImageDebugger` (branch `feat/wasm-vscode`); extension `/Users/bruno/ws/openimagedebugger-vscode` (branch `feat/live-update-local-wasm`).
- **Wire format:** `wasm32` — little-endian u32 headers/counts/lengths; UTF-8 length-prefixed strings.
- **Message types** (must match `src/ipc/message_exchange.h` and `src/ipc/message-exchange.ts`): 0–4 existing; **5** `ExportBufferRequest`; **6** `ApplySessionState`; **7** `SessionStateChanged`; **8** `ExportSelectedBuffer`.
- **Type 5 stub:** already ships variable name only; P5 extends with `int32 format` + `float32[8]` contrast — update OID media and extension atomically.
- **WASM nested event loops:** never call `QMenu::exec()` or `QFileDialog::exec()` under `__EMSCRIPTEN__`; use `popup()` and IPC.
- **Module imports (extension):** `.js` specifiers; compile before test (`npm run compile && npm test`).
- **Media sync:** after OID WASM rebuild, run `wasm/scripts/pack.sh` to copy artifacts into extension `media/`.
- **Persistence delay:** 100 ms debounce (matches desktop `SETTINGS_PERSIST_DELAY_MS`).
- **Buffer expiry:** 1 day (`BUFFER_EXPIRATION_DAYS` in `settings_manager.h`).

---

### Task 1: Extend ExportBufferRequest IPC (format + contrast)

**Files:**
- Modify: `OpenImageDebugger/src/ipc/message_exchange.h`
- Modify: `OpenImageDebugger/src/ui/messaging/message_handler.h`
- Modify: `OpenImageDebugger/src/ui/messaging/message_handler.cpp`
- Modify: `OpenImageDebugger/src/ui/events/event_handler.cpp`
- Modify: `openimagedebugger-vscode/src/ipc/message-exchange.ts`
- Test: `openimagedebugger-vscode/test/message-exchange.test.ts`

**Interfaces:**
- Consumes: existing `MessageComposer` / `decodeInbound`.
- Produces:
  - C++ `MessageHandler::request_export_buffer(const QString& name, int format, std::array<float,8> contrast)`
  - TS `decodeInbound` → `{ kind: 'exportRequest', name: string, format: number, contrast: Float32Array }`
  - TS `buildExportBufferRequest(...)` for tests only (viewer sends from C++)

- [ ] **Step 1: Add `float` to C++ `PrimitiveType` concept**

In `message_exchange.h`, extend the concept:

```cpp
concept PrimitiveType =
    std::is_same_v<T, MessageType> || std::is_same_v<T, int> ||
    std::is_same_v<T, float> ||
    /* ... existing ... */;
```

- [ ] **Step 2: Extend WASM export to send format + 8 floats**

In `event_handler.cpp` `#ifdef __EMSCRIPTEN__` block inside `export_buffer`, after resolving `Buffer` component:

```cpp
const auto* bc = component.auto_buffer_contrast_brightness();
std::array<float, 8> contrast{};
for (int i = 0; i < 8; ++i) contrast[i] = bc[i];
emit exportBufferRequested(buffer_name, 0, contrast); // 0 = PNG default from menu
```

Update signal to carry `(QString, int format, std::array<float,8>)` and `MessageHandler::request_export_buffer` to push all fields.

- [ ] **Step 3: Write failing TS decode test**

```ts
test('decodeInbound parses ExportBufferRequest with format and contrast', () => {
  const parts: number[] = [];
  const u32 = (v: number) => parts.push(v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff);
  const f32 = (v: number) => { const buf = new ArrayBuffer(4); new DataView(buf).setFloat32(0, v, true); parts.push(...new Uint8Array(buf)); };
  u32(MessageType.ExportBufferRequest);
  u32(3); parts.push('i'.charCodeAt(0), 'm'.charCodeAt(0), 'g'.charCodeAt(0)); // use TextEncoder in real test
  u32(1); // format Octave
  for (let i = 0; i < 8; i++) f32(i * 0.1);
  const msg = decodeInbound(new Uint8Array(parts));
  assert.equal(msg.kind, 'exportRequest');
  // assert name, format, contrast length
});
```

- [ ] **Step 4: Implement TS decoder + run tests**

Run: `cd openimagedebugger-vscode && npm run compile && npm test`  
Expected: PASS

- [ ] **Step 5: Rebuild WASM + commit both repos**

```bash
cd OpenImageDebugger && wasm/scripts/pack.sh --build
cd ../openimagedebugger-vscode && npm run compile && npm test
# commit OID + extension separately
git commit -m "feat(ipc): extend ExportBufferRequest with format and contrast"
```

---

### Task 2: Extension buffer exporter (PNG + Octave)

**Files:**
- Create: `openimagedebugger-vscode/src/export/buffer-exporter.ts`
- Create: `openimagedebugger-vscode/src/export/contrast.ts` (port `get_multiplier`, contrast math from `buffer_exporter.cpp`)
- Create: `openimagedebugger-vscode/test/buffer-exporter.test.ts`
- Modify: `openimagedebugger-vscode/package.json` (add `pngjs` dependency)

**Interfaces:**
- Consumes: `PlotBufferParams`, `Float32Array` contrast (8), save path, format enum.
- Produces: `exportBuffer(params: ExportParams): Promise<void>`

- [ ] **Step 1: Add `pngjs` and write failing Octave header test**

```ts
test('octaveHeader writes type descriptor and dimensions', () => {
  const header = buildOctaveHeader('uint8', 10, 20, 1);
  assert.match(header, /^uint81020/);
});
```

- [ ] **Step 2: Implement Octave binary writer + PNG path using pngjs**

Port row iteration from `export_binary` / `export_bitmap` in `src/io/buffer_exporter.cpp`.

- [ ] **Step 3: Wire `ViewerController.setExportBufferRequestHandler` → `exportBuffer`**

Replace info toast in `extension.ts` with:

```ts
viewer.setExportBufferRequestHandler((req) => {
  void handleExportRequest(req, stoppedThreadId, bridge, context);
});
```

`handleExportRequest`: `showSaveDialog` → `getBufferMetadata` → `exportBuffer`.

- [ ] **Step 4: Run tests and commit**

```bash
git commit -m "feat(export): add PNG and Octave buffer exporter in extension"
```

---

### Task 3: ExportSelectedBuffer + `oid.export` command

**Files:**
- Modify: `OpenImageDebugger/src/ipc/message_exchange.h` (type 8)
- Modify: `OpenImageDebugger/src/ui/messaging/message_handler.cpp` (decode 8)
- Modify: `openimagedebugger-vscode/src/ipc/message-exchange.ts` (`buildExportSelectedBuffer`)
- Modify: `openimagedebugger-vscode/src/webview/panel.ts`
- Modify: `openimagedebugger-vscode/package.json`
- Modify: `openimagedebugger-vscode/src/extension.ts`

**Interfaces:**
- Produces: command `oid.export`; `panel.requestExportSelected(): Promise<void>` sends type 8.

- [ ] **Step 1: OID decode type 8 → call `UIEventHandler::export_buffer(selectedName)`**

Track `selectedBuffer` from list selection (already in `buffer_data.currently_selected_stage` / list row).

- [ ] **Step 2: Register command in extension**

```json
{ "command": "oid.export", "title": "OID: Export Selected Buffer" }
```

- [ ] **Step 3: Manual test** — plot buffer, run command, save dialog appears, file written.

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(export): add oid.export command via ExportSelectedBuffer IPC"
```

---

### Task 4: Session persistence — extension module

**Files:**
- Create: `openimagedebugger-vscode/src/session/persistence.ts`
- Create: `openimagedebugger-vscode/test/persistence.test.ts`
- Modify: `openimagedebugger-vscode/src/extension.ts`

**Interfaces:**
- Produces:
  - `loadSession(context): OidSessionState`
  - `saveSession(context, state): void` (debounced)
  - `mergeSessionDelta(state, partial): OidSessionState`
  - `migrateWatchOnStop(state, names: string[]): OidSessionState`
  - `encodeSessionState(state): string` / `decodeSessionState(json): OidSessionState`

- [ ] **Step 1: Write failing tests for expiry + merge + migration**

- [ ] **Step 2: Implement persistence module matching spec §3 JSON schema**

Defaults: framerate 60, defaultSuffix `"Image File (*.png)"`, empty watched list.

- [ ] **Step 3: On viewer ready, load + migrate + send ApplySessionState (Task 5 codec)**

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(session): add extension-owned session persistence module"
```

---

### Task 5: Session persistence — IPC + WASM apply/persist

**Files:**
- Modify: `OpenImageDebugger/src/ipc/message_exchange.h` (types 6, 7)
- Modify: `OpenImageDebugger/src/ui/messaging/message_handler.cpp`
- Modify: `OpenImageDebugger/src/ui/main_window/main_window.cpp`
- Create: `OpenImageDebugger/src/ui/messaging/session_state_codec.cpp` (JSON parse → SettingsApplier calls; use Qt `QJsonDocument` or minimal parser)
- Modify: `openimagedebugger-vscode/src/ipc/message-exchange.ts`
- Modify: `openimagedebugger-vscode/src/webview/panel.ts`

**Interfaces:**
- Produces: `buildApplySessionState(json: string): Uint8Array`; decode type 7 in panel → `onSessionStateChanged(partial)`.

- [ ] **Step 1: Implement C++ decode ApplySessionState** — populate `buffer_data.previous_session_buffers`, emit same signals as `SettingsManager::load_*`.

- [ ] **Step 2: Replace EMSCRIPTEN `SettingsManager::persist_settings` path** — serialize `MainWindow::persist_settings` callbacks to JSON, send type 7.

- [ ] **Step 3: Skip QSettings load on WASM startup**

- [ ] **Step 4: Extension sends ApplySessionState on viewer-ready; handles SessionStateChanged**

- [ ] **Step 5: Rebuild WASM, pack media, manual test restart persistence**

- [ ] **Step 6: Commit both repos**

```bash
git commit -m "feat(session): sync session state over ApplySessionState IPC"
```

---

### Task 6: TypeBridge router + OpenCV legacy types

**Files:**
- Create: `openimagedebugger-vscode/src/typebridge/index.ts`
- Create: `openimagedebugger-vscode/src/typebridge/opencv-cvmat.ts`
- Create: `openimagedebugger-vscode/src/typebridge/opencv-iplimage.ts`
- Modify: `openimagedebugger-vscode/src/debugger/codelldb-bridge.ts`
- Test: `openimagedebugger-vscode/test/opencv-cvmat.test.ts`, `opencv-iplimage.test.ts`

**Interfaces:**
- Produces: `resolveInspector(type: string): TypeInspector | undefined`
- Produces: `TypeInspector.getBufferMetadata(name, frameId, dap): Promise<PlotBufferParams>`

- [ ] **Step 1: Port `CvMat` and `IplImage` decode from `resources/oidscripts/oidtypes/opencv.py`**

- [ ] **Step 2: Refactor `CodeLldbBridge` to evaluate type string first, dispatch via router**

- [ ] **Step 3: Tests + commit**

```bash
git commit -m "feat(typebridge): add CvMat and IplImage inspectors"
```

---

### Task 7: Eigen matrix inspector

**Files:**
- Create: `openimagedebugger-vscode/src/typebridge/eigen-matrix.ts`
- Create: `openimagedebugger-vscode/test/fixtures/eigen-*.json` (recorded LLDB evaluate outputs)
- Test: `openimagedebugger-vscode/test/eigen-matrix.test.ts`

- [ ] **Step 1: Port `eigen3.py` using DAP evaluate for Map/Matrix, dynamic dims, transpose**

- [ ] **Step 2: Register in router after OpenCV, before custom types**

- [ ] **Step 3: Commit**

```bash
git commit -m "feat(typebridge): add Eigen Matrix/Map inspector"
```

---

### Task 8: Custom types (`oid.customTypes`)

**Files:**
- Create: `openimagedebugger-vscode/src/typebridge/custom-type.ts`
- Modify: `openimagedebugger-vscode/package.json` (configuration schema)
- Modify: `openimagedebugger-vscode/src/typebridge/symbol-observable.ts`
- Test: `openimagedebugger-vscode/test/custom-type.test.ts`

- [ ] **Step 1: Define `CustomTypeConfig` interface matching spec §6**

- [ ] **Step 2: Load workspace configs at activation; prepend custom inspectors in router**

- [ ] **Step 3: Extend `isSymbolObservable` with custom patterns**

- [ ] **Step 4: Document example in extension README (optional one-liner in package.json description)**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(typebridge): add pluggable custom type inspectors"
```

---

### Task 9: Integration verification + media refresh

**Files:**
- Modify: `OpenImageDebugger/tests/test_message_exchange.cpp` (fix `bit_cast` on enum if needed, add type 5–8 round-trips)

- [ ] **Step 1: Full rebuild**

```bash
cd OpenImageDebugger && wasm/scripts/pack.sh --build
cd ../openimagedebugger-vscode && npm run compile && npm test
```

- [ ] **Step 2: Manual matrix (spec §9)**

- [ ] **Step 3: Final commits if any fixups**

```bash
git commit -m "chore(media): refresh WASM for P5 full parity"
```

---

## Plan self-review

| Spec requirement | Task |
|------------------|------|
| Export PNG/Octave hybrid | 1–3 |
| Re-fetch + contrast from WASM | 1–2 |
| Extension persistence | 4–5 |
| Full desktop UI prefs + buffers | 4–5 |
| Eigen + CvMat + IplImage | 6–7 |
| Custom types | 8 |
| Legacy IPC 5–8 | 1, 3, 5 |
| Testing | All tasks |

No TBD placeholders. Type/function names consistent across tasks (`exportBuffer`, `OidSessionState`, `buildApplySessionState`).
