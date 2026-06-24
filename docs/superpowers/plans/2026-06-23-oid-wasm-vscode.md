# OID WASM + VS Code Extension — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver full OID parity in VS Code on macOS/Linux by publishing a Qt6 WASM `oidwindow` artifact from this repo and a separate CodeLLDB-based TypeScript extension that speaks OBP v1 over `postMessage`.

**Architecture:** OID repo gains `ITransport` (TCP desktop / postMessage WASM), completes OBP v1 control messages, and builds `oidwindow` for Emscripten. A separate `openimagedebugger-vscode` repo hosts the DAP bridge, TypeBridge port, and webview loader consuming `@openimagedebugger/viewer-wasm`.

**Tech stack:** C++20, Qt 6.4+ (desktop + WASM kit), Emscripten 3.x, Protobuf 3, CMake 3.28+, GTest; extension: TypeScript, VS Code Extension API, `@bufbuild/protobuf` or `protobufjs`, CodeLLDB DAP.

**Design spec:** `docs/superpowers/specs/2026-06-23-oid-wasm-vscode-design.md`

---

## File map — OID repository (Part I)

| File | Role |
|------|------|
| `docs/protocol/obp/v1/control.proto` | Add `PlotBufferRequest`; extend `Envelope` |
| `src/ipc/transport.h` | `ITransport` interface |
| `src/ipc/tcp_transport.h` / `.cpp` | `QTcpSocket` adapter (desktop) |
| `src/ipc/postmessage_transport.h` / `.cpp` | Emscripten `EM_ASM` adapter (WASM) |
| `src/ipc/obp_channel.h` / `.cpp` | Serialize/deserialize `oid.obp.v1.Envelope` over `ITransport` |
| `src/ipc/message_exchange.h` / `.cpp` | Refactor to use `ITransport*` |
| `src/ui/messaging/message_handler.h` / `.cpp` | Accept `ITransport&` in `Dependencies` |
| `src/ui/main_window/main_window.h` | Replace `QTcpSocket socket_` with transport ownership |
| `src/oid_window.cpp` | `__EMSCRIPTEN__` entry: no TCP connect, postMessage handshake |
| `wasm/loader.html` | Qt WASM bootstrap + `window.oidSend` / `window.oidOnMessage` glue |
| `wasm/package.json` | `@openimagedebugger/viewer-wasm` npm metadata |
| `cmake/EmscriptenWasm.cmake` | Qt6 WASM toolchain helpers |
| `src/CMakeLists.txt` | Conditional `oidwindow` WASM target |
| `tests/test_transport.cpp` | `TcpTransport` round-trip tests |
| `tests/test_obp_channel.cpp` | Envelope encode/decode tests |
| `.github/workflows/wasm.yml` | Emscripten + Qt WASM CI build |

## File map — extension repository (Part II, separate repo)

| File | Role |
|------|------|
| `package.json` | Extension manifest, commands, `activationEvents` |
| `src/extension.ts` | Activation, debug session tracking |
| `src/debugger/codelldb-bridge.ts` | `BridgeInterface` port |
| `src/debugger/dap-client.ts` | `evaluate`, `readMemory`, stop events |
| `src/typebridge/opencv-mat.ts` | OpenCV `Mat` metadata extraction |
| `src/obp/` | Generated TS from OID protos |
| `src/webview/panel.ts` | `WebviewPanel` host + postMessage relay |
| `src/session/session-manager.ts` | Stop handler, watched buffers |
| `media/` | Copied WASM artifacts from npm package |

---

# Part I — OID repository (P0–P1)

### Task 1: Add `PlotBufferRequest` to OBP v1

**Files:**
- Modify: `docs/protocol/obp/v1/control.proto`
- Modify: `docs/protocol/obp/v1/conformance/README.md` (document new message)
- Test: `tests/test_obp_conformance.cpp`

- [ ] **Step 1: Add message and envelope field**

```protobuf
message PlotBufferRequest {
  string variable_name = 1;
}

// Inside Envelope.oneof body, add:
//   PlotBufferRequest plot_buffer_request = 18;
```

- [ ] **Step 2: Rebuild and run existing OBP tests**

Run: `cmake -S . -B build -DBUILD_TESTS=ON && cmake --build build --target test_obp_conformance && ctest --test-dir build -R ObpConformance -V`

Expected: PASS (no regression)

- [ ] **Step 3: Add conformance case** — create `docs/protocol/obp/v1/conformance/cases/plot_buffer_request.pb` (binary fixture) and assert decode in `tests/test_obp_conformance.cpp`:

```cpp
TEST(ObpConformance, PlotBufferRequestRoundTrip) {
  oid::obp::v1::Envelope env;
  env.set_protocol_version("1");
  env.mutable_plot_buffer_request()->set_variable_name("img");
  std::string bytes;
  ASSERT_TRUE(env.SerializeToString(&bytes));
  oid::obp::v1::Envelope parsed;
  ASSERT_TRUE(parsed.ParseFromString(bytes));
  EXPECT_EQ(parsed.plot_buffer_request().variable_name(), "img");
}
```

- [ ] **Step 4: Commit**

```bash
git add docs/protocol/obp/v1/control.proto docs/protocol/obp/v1/conformance/ tests/test_obp_conformance.cpp
git commit -m "feat(obp): add PlotBufferRequest envelope message"
```

---

### Task 2: Introduce `ITransport` interface

**Files:**
- Create: `src/ipc/transport.h`
- Create: `src/ipc/tcp_transport.h`
- Create: `src/ipc/tcp_transport.cpp`
- Modify: `src/ipc/CMakeLists.txt`
- Test: `tests/test_transport.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing transport test**

```cpp
// tests/test_transport.cpp
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <thread>
#include "ipc/tcp_transport.h"

TEST(TcpTransport, SendsAndReceivesBytes) {
  int argc = 0;
  char* argv = nullptr;
  QCoreApplication app(argc, argv);
  QTcpServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  const auto port = server.serverPort();

  std::vector<std::byte> received;
  std::thread server_thread([&] {
    if (!server.waitForNewConnection(5000)) return;
    auto* sock = server.nextPendingConnection();
    sock->waitForReadyRead(5000);
    const auto data = sock->readAll();
    received.assign(reinterpret_cast<const std::byte*>(data.constData()),
                    reinterpret_cast<const std::byte*>(data.constData()) + data.size());
    sock->write(data);
    sock->waitForBytesWritten(5000);
    sock->close();
  });

  QTcpSocket client;
  client.connectToHost(QHostAddress::LocalHost, port);
  ASSERT_TRUE(client.waitForConnected(5000));
  oid::TcpTransport transport(client);

  const std::vector<std::byte> payload{std::byte{0x01}, std::byte{0x02}};
  transport.send(payload);
  ASSERT_TRUE(client.waitForReadyRead(5000));
  std::vector<std::byte> out;
  ASSERT_TRUE(transport.receive(out));
  EXPECT_EQ(out, payload);

  server_thread.join();
}
```

- [ ] **Step 2: Run test — expect link failure**

Run: `cmake --build build --target test_transport 2>&1 | tail -5`

Expected: undefined reference / file not found

- [ ] **Step 3: Implement `transport.h` and `TcpTransport`**

```cpp
// src/ipc/transport.h
#pragma once
#include <cstddef>
#include <span>
#include <vector>

namespace oid {

class ITransport {
 public:
  virtual ~ITransport() = default;
  virtual void send(std::span<const std::byte> data) = 0;
  virtual bool receive(std::vector<std::byte>& out) = 0;
};

}  // namespace oid
```

```cpp
// src/ipc/tcp_transport.h
#pragma once
#include "ipc/transport.h"
class QTcpSocket;

namespace oid {
class TcpTransport final : public ITransport {
 public:
  explicit TcpTransport(QTcpSocket& socket);
  void send(std::span<const std::byte> data) override;
  bool receive(std::vector<std::byte>& out) override;
 private:
  QTcpSocket& socket_;
};
}  // namespace oid
```

`send` writes length-prefixed or raw bytes matching existing `MessageComposer` wire format (match current `MessageComposer::send(QTcpSocket*)` framing exactly — read `message_exchange.h:169` before implementing).

- [ ] **Step 4: Wire into `src/ipc/CMakeLists.txt` and `tests/CMakeLists.txt`**

- [ ] **Step 5: Run test — expect PASS**

Run: `ctest --test-dir build -R TcpTransport -V`

- [ ] **Step 6: Commit**

```bash
git commit -m "feat(ipc): add ITransport and TcpTransport"
```

---

### Task 3: Refactor `MessageComposer` / `MessageDecoder` to use `ITransport`

**Files:**
- Modify: `src/ipc/message_exchange.h`
- Modify: `src/ipc/message_exchange.cpp` (if split from header)
- Test: `tests/test_message_exchange.cpp`

- [ ] **Step 1: Add overloads without removing old API yet**

```cpp
void send(ITransport* transport) const;
explicit MessageDecoder(ITransport* transport);
```

Keep `send(QTcpSocket*)` as inline wrapper:

```cpp
void send(QTcpSocket* socket) const {
  TcpTransport transport(*socket);
  send(&transport);
}
```

- [ ] **Step 2: Run existing message exchange tests**

Run: `ctest --test-dir build -R MessageExchange -V`

Expected: PASS (no behavior change)

- [ ] **Step 3: Commit**

```bash
git commit -m "refactor(ipc): MessageComposer accepts ITransport"
```

---

### Task 4: Add `ObpChannel` for Envelope over transport

**Files:**
- Create: `src/ipc/obp_channel.h`
- Create: `src/ipc/obp_channel.cpp`
- Modify: `src/ipc/CMakeLists.txt`
- Test: `tests/test_obp_channel.cpp`

- [ ] **Step 1: Write failing test**

```cpp
TEST(ObpChannel, RoundTripPlotBufferRequest) {
  // Use two TcpTransport ends over QTcpSocket pair (see test_transport pattern)
  oid::obp::v1::Envelope env;
  env.set_protocol_version("1");
  env.set_sequence(1);
  env.mutable_plot_buffer_request()->set_variable_name("frame");

  oid::ObpChannel sender(transport_a);
  oid::ObpChannel receiver(transport_b);
  sender.send(env);

  oid::obp::v1::Envelope got;
  ASSERT_TRUE(receiver.receive(got));
  EXPECT_EQ(got.plot_buffer_request().variable_name(), "frame");
}
```

- [ ] **Step 2: Implement `ObpChannel`** — length-prefix `uint32_t` + serialized `Envelope` bytes; link `obp` proto lib.

- [ ] **Step 3: Run test — expect PASS**

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(ipc): add ObpChannel for OBP Envelope transport"
```

---

### Task 5: Wire `MessageHandler` to OBP for control messages (WASM path)

**Files:**
- Modify: `src/ui/messaging/message_handler.h` — add `ITransport* obp_transport` to `Dependencies`
- Modify: `src/ui/messaging/message_handler.cpp` — handle inbound `Envelope`:
  - `symbol_list` → existing `decode_set_available_symbols` logic
  - `plot_buffer` → call existing OBP decode path (`obp_decode.cpp`)
  - `session_state` → restore watched buffers
  - outbound `plot_buffer_request` → send `Envelope` instead of legacy `MessageType::PlotBufferRequest`
- Modify: `src/ui/main_window/main_window.h` / `main_window_initializer.cpp`
- Test: extend `tests/test_obp_conformance.cpp` or add integration test with mock transport

- [ ] **Step 1: Add compile flag `OID_ENABLE_OBP_TRANSPORT` (default ON for WASM, optional on desktop)**

- [ ] **Step 2: Implement inbound `Envelope` dispatch in `MessageHandler::process_messages`**

- [ ] **Step 3: Desktop TCP path still works** — run full test suite:

Run: `ctest --test-dir build -V`

Expected: all existing tests PASS

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(ui): route control messages through OBP Envelope"
```

---

### Task 6: `PostMessageTransport` (Emscripten)

**Files:**
- Create: `src/ipc/postmessage_transport.h`
- Create: `src/ipc/postmessage_transport.cpp`
- Create: `wasm/oid_postmessage.js` — `window.oidSend = Module.cwrap(...)` glue
- Modify: `wasm/loader.html`
- Test: `tests/test_postmessage_transport.cpp` (host-only mock; skip on Emscripten)

- [ ] **Step 1: Implement ring buffer + `EM_ASM` send/receive**

```cpp
#ifdef __EMSCRIPTEN__
void PostMessageTransport::send(std::span<const std::byte> data) {
  EM_ASM({
    window.oidHostSend(HEAPU8.slice($0, $0 + $1));
  }, data.data(), data.size());
}
#endif
```

- [ ] **Step 2: Host-side mock test** — inject bytes via test hook without browser

- [ ] **Step 3: Commit**

```bash
git commit -m "feat(ipc): add PostMessageTransport for WASM"
```

---

### Task 7: Emscripten CMake toolchain + WASM `oidwindow` target

**Files:**
- Create: `cmake/EmscriptenWasm.cmake`
- Create: `cmake/toolchains/emscripten-qt6.cmake` (document Qt WASM kit path via `Qt6_DIR`)
- Modify: `src/CMakeLists.txt`
- Modify: `src/oid_window.cpp`

- [ ] **Step 1: Add option `OID_BUILD_WASM` (default OFF)**

```cmake
option(OID_BUILD_WASM "Build oidwindow for WebAssembly" OFF)
if(OID_BUILD_WASM)
  include(${CMAKE_SOURCE_DIR}/cmake/EmscriptenWasm.cmake)
  add_subdirectory(wasm)
endif()
```

- [ ] **Step 2: WASM-specific compile defs**

```cmake
target_compile_definitions(oidwindow PRIVATE OID_TRANSPORT_POSTMESSAGE)
```

- [ ] **Step 3: Modify `oid_window.cpp` for Emscripten**

```cpp
#ifdef __EMSCRIPTEN__
  // No QCommandLineParser TCP connect
  // Construct PostMessageTransport, pass to MainWindow
  // EM_ASM({ window.parent.postMessage({type:'oid-ready'}, '*'); });
#else
  // existing TCP path
#endif
```

- [ ] **Step 4: Document local build in `README.md` (WASM section)**

```bash
# Prerequisites: Qt 6.x WASM kit, emsdk 3.x
source /path/to/emsdk/emsdk_env.sh
cmake -S . -B build-wasm -DOID_BUILD_WASM=ON \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/emscripten.cmake \
  -DQt6_DIR=/path/to/qt6-wasm/lib/cmake/Qt6
cmake --build build-wasm --target oidwindow -j
```

- [ ] **Step 5: Commit**

```bash
git commit -m "build: add Emscripten WASM target for oidwindow"
```

---

### Task 8: npm package `@openimagedebugger/viewer-wasm`

**Files:**
- Create: `wasm/package.json`
- Create: `wasm/loader.html`
- Create: `scripts/pack-viewer-wasm.sh`
- Modify: `.github/workflows/wasm.yml`

- [ ] **Step 1: `wasm/package.json`**

```json
{
  "name": "@openimagedebugger/viewer-wasm",
  "version": "0.0.0-dev",
  "description": "Qt6 WASM build of oidwindow for VS Code",
  "files": ["dist/"],
  "oid": {
    "protocol_version": "1",
    "min_extension_version": "0.1.0"
  }
}
```

- [ ] **Step 2: `pack-viewer-wasm.sh`** copies `build-wasm/oidwindow.{wasm,js}`, Qt runtime, `loader.html` → `wasm/dist/`

- [ ] **Step 3: CI workflow** — Ubuntu job installs emsdk + Qt WASM, builds, runs `npm pack` artifact upload

- [ ] **Step 4: Commit**

```bash
git commit -m "ci: package viewer-wasm npm artifact"
```

---

### Task 9: WASM smoke test (headless)

**Files:**
- Create: `wasm/smoke/playwright.config.ts`
- Create: `wasm/smoke/render.spec.ts`
- Modify: `.github/workflows/wasm.yml`

- [ ] **Step 1: Playwright test loads `loader.html`, posts sample `PlotBuffer` fixture from `docs/protocol/obp/v1/conformance/cases/`**

```typescript
test('wasm viewer accepts PlotBuffer', async ({ page }) => {
  await page.goto('http://localhost:8765/loader.html');
  await page.waitForFunction(() => (window as any).oidViewerReady === true);
  const fixture = fs.readFileSync('cases/sample_uint8_4x4.pb');
  await page.evaluate((bytes) => {
    window.oidHostSend(new Uint8Array(bytes));
  }, fixture);
  await expect(page.locator('#qtcanvas')).toBeVisible();
});
```

- [ ] **Step 2: Wire into CI after WASM build**

- [ ] **Step 3: Commit**

```bash
git commit -m "test(wasm): add Playwright smoke test for PlotBuffer render"
```

---

# Part II — Extension repository (P2–P6)

> Implement in **`openimagedebugger-vscode`** (new repo). Pin `@openimagedebugger/viewer-wasm` to OID release tags.

### Task 10: Scaffold extension repo

**Files:**
- Create: `openimagedebugger-vscode/package.json`
- Create: `openimagedebugger-vscode/tsconfig.json`
- Create: `openimagedebugger-vscode/src/extension.ts`
- Create: `openimagedebugger-vscode/.vscode/launch.json`

- [ ] **Step 1: `package.json` manifest**

```json
{
  "name": "openimagedebugger",
  "displayName": "Open Image Debugger",
  "version": "0.1.0",
  "engines": { "vscode": "^1.85.0" },
  "activationEvents": ["onDebug"],
  "main": "./out/extension.js",
  "contributes": {
    "commands": [
      { "command": "oid.openPanel", "title": "Open Image Debugger" },
      { "command": "oid.plot", "title": "Plot Image Buffer" }
    ]
  },
  "dependencies": {
    "@openimagedebugger/viewer-wasm": "1.0.0"
  },
  "devDependencies": {
    "@types/vscode": "^1.85.0",
    "typescript": "^5.4.0",
    "@vscode/test-electron": "^2.3.0"
  }
}
```

- [ ] **Step 2: Minimal `extension.ts` activates and registers `oid.openPanel`**

- [ ] **Step 3: Verify F5 launch opens Extension Development Host**

- [ ] **Step 4: Commit in extension repo**

```bash
git commit -m "chore: scaffold openimagedebugger-vscode extension"
```

---

### Task 11: OBP TypeScript codegen

**Files:**
- Create: `openimagedebugger-vscode/buf.gen.yaml` or `scripts/gen-obp.sh`
- Create: `openimagedebugger-vscode/src/obp/envelope.ts` (generated)
- Create: `openimagedebugger-vscode/src/obp/codec.ts`

- [ ] **Step 1: Vendor protos from OID** — git submodule `proto/` → `docs/protocol/obp/v1/*.proto`

- [ ] **Step 2: Generate TS with `@bufbuild/protobuf`**

```typescript
// src/obp/codec.ts
export interface OidMessage {
  type: 'obp';
  sequence: number;
  payload: Uint8Array;
}

export function encodeEnvelope(env: Envelope): OidMessage {
  return { type: 'obp', sequence: Number(env.sequence), payload: env.toBinary() };
}
```

- [ ] **Step 3: Unit test round-trip `PlotBufferRequest`**

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(obp): add TypeScript protobuf codec"
```

---

### Task 12: Webview panel + WASM loader

**Files:**
- Create: `openimagedebugger-vscode/src/webview/panel.ts`
- Create: `openimagedebugger-vscode/src/webview/loader.html` (from npm package template)
- Modify: `openimagedebugger-vscode/src/extension.ts`

- [ ] **Step 1: `OidWebviewPanel.create()`** — `retainContextWhenHidden: true`, copy wasm assets to `media/`

- [ ] **Step 2: postMessage relay**

```typescript
panel.webview.onDidReceiveMessage((msg: OidMessage) => {
  if (msg.type === 'oid-ready') sessionManager.onViewerReady();
  else if (msg.type === 'obp') sessionManager.handleEnvelope(decodeEnvelope(msg));
});
```

- [ ] **Step 3: Manual test** — command opens panel, WASM loads, console shows `oid-ready`

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(webview): load viewer-wasm in WebviewPanel"
```

---

### Task 13: CodeLLDB DAP bridge

**Files:**
- Create: `openimagedebugger-vscode/src/debugger/dap-client.ts`
- Create: `openimagedebugger-vscode/src/debugger/codelldb-bridge.ts`
- Test: `openimagedebugger-vscode/test/fixtures/evaluate-mat.json`
- Test: `openimagedebugger-vscode/test/codelldb-bridge.test.ts`

- [ ] **Step 1: Write failing test with fixture**

```typescript
test('getBufferMetadata parses cv::Mat evaluate result', async () => {
  const bridge = new CodeLldbBridge(mockDapClient(fixtures.matEvaluate));
  const meta = await bridge.getBufferMetadata('img');
  expect(meta.width).toBe(640);
  expect(meta.height).toBe(480);
  expect(meta.channels).toBe(3);
});
```

- [ ] **Step 2: Implement `evaluate` + `readMemory` in `dap-client.ts`**

Port logic from `resources/oidscripts/debuggers/lldbbridge.py` `get_buffer_metadata`.

- [ ] **Step 3: Implement `queueRequest` as microtask queue on extension host**

- [ ] **Step 4: Run test — expect PASS**

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(debugger): add CodeLLDB DAP bridge"
```

---

### Task 14: OpenCV Mat TypeBridge + PlotBuffer serializer

**Files:**
- Create: `openimagedebugger-vscode/src/typebridge/opencv-mat.ts`
- Create: `openimagedebugger-vscode/src/obp/plot-buffer.ts`
- Port from: `resources/oidscripts/obp/wire_encode.py`, `resources/oidscripts/types/mat.py`

- [ ] **Step 1: Port tile splitter** — reuse test vectors from `tests/test_obp_tile_splitter.py`

```typescript
export function encodePlotBuffer(metadata: BufferMetadata, maxTextureSize = 8192): Uint8Array {
  // mirror Python encode_plot_buffer
}
```

- [ ] **Step 2: Unit test against `docs/protocol/obp/v1/conformance/cases/sample_uint8_4x4.pb`**

- [ ] **Step 3: Commit**

```bash
git commit -m "feat(typebridge): OpenCV Mat to OBP PlotBuffer"
```

---

### Task 15: Session manager — stop handler + auto-update (P2/P3)

**Files:**
- Create: `openimagedebugger-vscode/src/session/session-manager.ts`
- Create: `openimagedebugger-vscode/src/session/persistence.ts`
- Modify: `openimagedebugger-vscode/src/extension.ts`

- [ ] **Step 1: Register `vscode.debug.onDidReceiveDebugSessionCustomEvent` or `DebugAdapterTracker` for `stopped`**

- [ ] **Step 2: Port `OpenImageDebuggerEvents.stop_handler`**

```typescript
async onStopped(session: vscode.DebugSession) {
  await this.ensurePanelReady();
  const symbols = await this.bridge.getAvailableSymbols();
  this.webview.postEnvelope(symbolListUpdate(symbols));
  for (const name of this.watchedBuffers) {
    const meta = await this.bridge.getBufferMetadata(name);
    this.webview.postEnvelope(plotBuffer(encodePlotBuffer(meta)));
  }
}
```

- [ ] **Step 3: Handle inbound `plot_buffer_request` from WASM**

- [ ] **Step 4: Integration test** — debug `testbench` binary, hit breakpoint, assert webview received `PlotBuffer`

- [ ] **Step 5: Commit**

```bash
git commit -m "feat(session): auto-update buffers on breakpoint"
```

---

### Task 16: UI parity features (P4)

**Files:**
- WASM/OBP only — no extension changes for zoom/pan (already in `oidwindow`)
- Modify: `openimagedebugger-vscode/src/session/session-manager.ts` — handle `session_state` sync
- Modify: `src/ui/messaging/message_handler.cpp` (OID) — emit `session_state` on UI changes

- [ ] **Step 1: Bidirectional `SessionState` sync** (contrast, rotation, link-views, watched list)

- [ ] **Step 2: Manual test checklist** — zoom, pan, go-to, pixel values, link views, rotate, auto-contrast

- [ ] **Step 3: Commit (both repos as needed)**

---

### Task 17: Export + persistence (P5)

**Files:**
- Modify: `openimagedebugger-vscode/src/session/session-manager.ts`
- OID: ensure `ExportRequest` / `ExportResult` wired in `message_handler.cpp`

- [ ] **Step 1: WASM sends `ExportRequest`; extension shows `showSaveDialog` and writes PNG**

- [ ] **Step 2: Persist `SessionState` to `context.globalState`**

- [ ] **Step 3: Port Eigen + custom types** — `src/typebridge/eigen-matrix.ts`, workspace `oid.customTypes` config

- [ ] **Step 4: Commit**

```bash
git commit -m "feat: export buffers and persist session state"
```

---

### Task 18: Ship (P6)

**Files:**
- Create: `openimagedebugger-vscode/README.md`
- Modify: `.github/workflows/release.yml` (extension repo)
- Modify: OID `README.md` — link to extension

- [ ] **Step 1: `vsce package` produces `.vsix`**

- [ ] **Step 2: Manual test matrix** — macOS + Linux, CodeLLDB, OpenCV Mat

- [ ] **Step 3: Publish extension to VS Code Marketplace**

- [ ] **Step 4: Tag OID release + npm `@openimagedebugger/viewer-wasm`**

---

## Verify commands (OID repo)

```bash
cmake -S . -B build -DBUILD_TESTS=ON -G Ninja
cmake --build build -j
ctest --test-dir build -V
# WASM (when toolchain available):
cmake -S . -B build-wasm -DOID_BUILD_WASM=ON -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
cmake --build build-wasm --target oidwindow -j
```

## Verify commands (extension repo)

```bash
npm install
npm test
npm run compile
# F5 in VS Code → Extension Development Host
# launch.json debugs testbench with CodeLLDB
```

---

## Plan self-review

- **Spec coverage:** P0 → Tasks 1–6; P1 → Tasks 7–9; P2 → Tasks 10–15; P3 → Task 15; P4 → Task 16; P5 → Task 17; P6 → Task 18. All spec sections mapped.
- **Placeholder scan:** No TBD steps; each task names concrete files and commands.
- **Type consistency:** `OidMessage`, `Envelope`, `ITransport`, `ObpChannel` used consistently across OID and extension tasks.
- **Scope:** Part I delivers testable WASM artifact; Part II delivers working extension against that artifact.
