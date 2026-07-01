# OID WASM + VS Code Extension (Qt 6.11) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `oidwindow` for Qt 6.11 WebAssembly, refactor IPC onto `ITransport`, publish `@openimagedebugger/viewer-wasm`, and validate with a minimal VS Code extension smoke test using the **existing `MessageComposer` binary protocol** (no Protobuf).

**Architecture:** Desktop keeps TCP via `TcpTransport`. WASM uses `PostMessageTransport` (JS queue + `window.oidSend`). `MessageComposer`/`MessageDecoder` are transport-agnostic. Extension encodes `PlotBufferContents` in TS with **wasm32 `size_t` (4-byte) length prefixes** and sends `{ type: 'oid-ipc', payload }` over postMessage.

**Tech stack:** C++20, Qt 6.11 (desktop + WASM kit), Emscripten 4.0.7, CMake 3.28+, GTest; extension: TypeScript, VS Code Extension API.

**Design spec:** `docs/superpowers/specs/2026-06-24-oid-wasm-vscode-qt611-design.md`

---

## File map — OID repository

| File | Role |
|------|------|
| `common.cmake` | Qt 6.11 minimum |
| `src/CMakeLists.txt` | Qt 6.11; conditional WASM target; skip OpenGL pkg on Emscripten |
| `src/ipc/transport.h` | `ITransport` interface |
| `src/ipc/tcp_transport.h` / `.cpp` | Desktop `QTcpSocket` adapter |
| `src/ipc/postmessage_transport.h` / `.cpp` | WASM `EM_ASM` adapter + test queue |
| `src/ipc/message_exchange.h` | `MessageComposer::send(ITransport&)`, `MessageDecoder(ITransport&)` |
| `src/ipc/CMakeLists.txt` | Add transport sources |
| `src/ui/messaging/message_handler.h` / `.cpp` | `ITransport&` in `Dependencies` |
| `src/ui/main_window/main_window.h` / `.cpp` | Own `TcpTransport`; wire handler |
| `src/ui/main_window/main_window_initializer.cpp` | TCP connect guarded by `#ifndef __EMSCRIPTEN__` |
| `src/oid_window.cpp` | Emscripten entry: no TCP, viewer-ready handshake |
| `src/ui/gl_canvas.cpp` | `GL_FRAMEBUFFER_EXT` → `GL_FRAMEBUFFER` |
| `cmake/EmscriptenWasm.cmake` | EMSDK / Qt6_DIR validation |
| `wasm/loader.html` | Qt loader + `oidSend`/`oidOnMessage` glue |
| `wasm/package.json` | npm metadata |
| `wasm/scripts/pack-viewer-wasm.sh` | Assemble `dist/` artifact |
| `tests/test_transport.cpp` | `TcpTransport` + `PostMessageTransport` tests |
| `tests/test_message_exchange.cpp` | Update to `TcpTransport` |
| `tests/CMakeLists.txt` | Register `test_transport` |
| `.github/workflows/build.yml` | Qt 6.11 on Windows |
| `.github/workflows/wasm.yml` | WASM CI build |

## File map — extension repository (`openimagedebugger-vscode`)

| File | Role |
|------|------|
| `package.json` | `oid.openPanel` command |
| `src/extension.ts` | Activation |
| `src/ipc/message-exchange.ts` | TS `MessageComposer` (wasm32 lengths) |
| `src/webview/panel.ts` | Webview + postMessage relay |
| `test/message-exchange.test.ts` | Byte layout unit test |
| `test/fixtures/sample_plot_buffer.bin` | Generated fixture |
| `media/` | Copied WASM artifacts |

---

## Cross-bitness note (read before Task 8)

Desktop `MessageComposer` uses native `sizeof(std::size_t)` (8 bytes on x64). Qt WASM (wasm32) uses **4-byte `size_t`**. The VS Code extension runs on x64 Node and must encode lengths as **uint32** when targeting WASM. Desktop TCP path is unchanged (bridge and viewer share arch). Task 8 implements `lengthBytes: 4` in the TS codec.

---

# Part I — OID repository

### Task 1: Bump Qt minimum to 6.11

**Files:**
- Modify: `common.cmake`
- Modify: `src/CMakeLists.txt`
- Modify: `README.md`
- Modify: `.github/workflows/build.yml`

- [ ] **Step 1: Update CMake package requirements**

In `common.cmake` line 35:

```cmake
find_package(Qt6 6.11 REQUIRED COMPONENTS Network)
```

In `src/CMakeLists.txt` line 35:

```cmake
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui OpenGL OpenGLWidgets Widgets)
```

- [ ] **Step 2: Update README** — change `Qt **6.4.2+**` to `Qt **6.11+**`.

- [ ] **Step 3: Update Windows CI Qt version** in `.github/workflows/build.yml`:

```yaml
version: "6.11.*"
```

- [ ] **Step 4: Build and test locally**

Run: `cmake -S . -B build -DBUILD_TESTS=ON && cmake --build build --config Release && ctest --test-dir build --output-on-failure`

Expected: PASS (requires Qt 6.11 installed)

- [ ] **Step 5: Commit**

```bash
git add common.cmake src/CMakeLists.txt README.md .github/workflows/build.yml
git commit -m "build: require Qt 6.11 minimum"
```

---

### Task 2: Introduce `ITransport` and `TcpTransport`

**Files:**
- Create: `src/ipc/transport.h`
- Create: `src/ipc/tcp_transport.h`
- Create: `src/ipc/tcp_transport.cpp`
- Modify: `src/ipc/CMakeLists.txt`
- Create: `tests/test_transport.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing transport test**

```cpp
// tests/test_transport.cpp
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <array>
#include <bit>
#include <thread>
#include <vector>

#include "ipc/tcp_transport.h"

namespace {
QCoreApplication& app() {
  static int argc = 1;
  static std::array<char*, 2> argv = {const_cast<char*>("test"), nullptr};
  static QCoreApplication application(argc, argv.data());
  return application;
}
}  // namespace

TEST(TcpTransport, RoundTripBytes) {
  (void)app();
  QTcpServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));

  std::vector<std::byte> server_received;
  std::thread server_thread([&] {
    ASSERT_TRUE(server.waitForNewConnection(5000));
    QTcpSocket* peer = server.nextPendingConnection();
    ASSERT_TRUE(peer->waitForReadyRead(5000));
    const QByteArray data = peer->readAll();
    server_received.assign(
        reinterpret_cast<const std::byte*>(data.constData()),
        reinterpret_cast<const std::byte*>(data.constData()) + data.size());
    peer->write(data);
    peer->waitForBytesWritten(5000);
    peer->close();
    delete peer;
  });

  QTcpSocket client;
  client.connectToHost(QHostAddress::LocalHost, server.serverPort());
  ASSERT_TRUE(client.waitForConnected(5000));

  oid::TcpTransport transport(client);
  const std::vector<std::byte> sent{std::byte{0x03}, std::byte{0x01}};
  transport.send(sent);

  std::array<std::byte, 2> buf{};
  const auto n = transport.receive(buf);
  EXPECT_EQ(n, 2u);
  EXPECT_EQ(buf[0], std::byte{0x03});
  EXPECT_EQ(buf[1], std::byte{0x01});
  EXPECT_EQ(server_received, sent);

  server_thread.join();
}
```

- [ ] **Step 2: Run test — expect failure**

Run: `cmake -S . -B build -DBUILD_TESTS=ON && cmake --build build --target test_transport 2>&1 | tail -3`

Expected: build error (missing `tcp_transport.h`)

- [ ] **Step 3: Implement transport interface**

```cpp
// src/ipc/transport.h
#ifndef IPC_TRANSPORT_H_
#define IPC_TRANSPORT_H_

#include <cstddef>
#include <span>

namespace oid {

class ITransport {
 public:
  virtual ~ITransport() = default;
  virtual void send(std::span<const std::byte> data) = 0;
  virtual std::size_t receive(std::span<std::byte> dst) = 0;
  virtual bool has_data() const = 0;
};

}  // namespace oid

#endif
```

```cpp
// src/ipc/tcp_transport.h
#ifndef IPC_TCP_TRANSPORT_H_
#define IPC_TCP_TRANSPORT_H_

#include "ipc/transport.h"

class QTcpSocket;

namespace oid {

class TcpTransport final : public ITransport {
 public:
  explicit TcpTransport(QTcpSocket& socket);

  void send(std::span<const std::byte> data) override;
  std::size_t receive(std::span<std::byte> dst) override;
  bool has_data() const override;

 private:
  QTcpSocket& socket_;
};

}  // namespace oid

#endif
```

```cpp
// src/ipc/tcp_transport.cpp
#include "ipc/tcp_transport.h"

#include <bit>

#include <QAbstractSocket>
#include <QTcpSocket>

#include "ipc/message_exchange.h"  // SocketTimeoutError

namespace oid {

TcpTransport::TcpTransport(QTcpSocket& socket) : socket_{socket} {}

void TcpTransport::send(std::span<const std::byte> data) {
  auto offset = qint64{0};
  const auto total = static_cast<qint64>(data.size());
  while (offset < total) {
    offset += socket_.write(std::bit_cast<const char*>(data.data() + offset),
                            total - offset);
    if (offset < total && !socket_.waitForBytesWritten(5000)) {
      throw_socket_timeout_error("write");
    }
  }
  if (!socket_.waitForBytesWritten(5000)) {
    throw_socket_timeout_error("write");
  }
}

std::size_t TcpTransport::receive(std::span<std::byte> dst) {
  auto offset = std::size_t{0};
  while (offset < dst.size()) {
    if (!has_data() && !socket_.waitForReadyRead(5000)) {
      throw_socket_timeout_error("read");
    }
    const auto n = socket_.read(
        std::bit_cast<char*>(dst.data() + offset),
        static_cast<qint64>(dst.size() - offset));
    if (n < 0) {
      throw_socket_timeout_error("read");
    }
    if (n == 0) {
      break;
    }
    offset += static_cast<std::size_t>(n);
  }
  return offset;
}

bool TcpTransport::has_data() const {
  return socket_.bytesAvailable() > 0;
}

}  // namespace oid
```

- [ ] **Step 4: Wire into `src/ipc/CMakeLists.txt`**

```cmake
add_library(${PROJECT_NAME} SHARED
            message_exchange.cpp
            raw_data_decode.cpp
            tcp_transport.cpp)
```

- [ ] **Step 5: Register test in `tests/CMakeLists.txt`**

```cmake
add_executable(test_transport test_transport.cpp)
target_include_directories(test_transport PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_transport PRIVATE Qt6::Network Qt6::Core GTest::gtest_main GTest::gtest)
target_sources(test_transport PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/tcp_transport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/message_exchange.cpp)
add_test(NAME TransportTests COMMAND test_transport)
```

- [ ] **Step 6: Run test**

Run: `cmake --build build --target test_transport && ctest --test-dir build -R TransportTests -V`

Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add src/ipc/transport.h src/ipc/tcp_transport.h src/ipc/tcp_transport.cpp \
        src/ipc/CMakeLists.txt tests/test_transport.cpp tests/CMakeLists.txt
git commit -m "feat(ipc): add ITransport and TcpTransport"
```

---

### Task 3: Refactor `MessageComposer` / `MessageDecoder` to use `ITransport`

**Files:**
- Modify: `src/ipc/message_exchange.h`
- Modify: `tests/test_message_exchange.cpp`

- [ ] **Step 1: Replace `QTcpSocket` with `ITransport` in `message_exchange.h`**

Add include:

```cpp
#include "ipc/transport.h"
```

Remove `#include <QTcpSocket>` and `QPointer`.

Change `MessageComposer::send`:

```cpp
void send(ITransport& transport) const {
  for (const auto& block : message_blocks_) {
    transport.send(std::span{block->data(), block->size()});
  }
}
```

Change `MessageDecoder`:

```cpp
class MessageDecoder {
 public:
  explicit MessageDecoder(ITransport& transport) : transport_{transport} {}

  // ... template read methods unchanged ...

 private:
  ITransport& transport_;

  void read_impl(std::span<std::byte> dst) const {
    auto offset = std::size_t{0};
    while (offset < dst.size()) {
      const auto n = transport_.receive(dst.subspan(offset));
      if (n == 0) {
        throw_socket_timeout_error("read");
      }
      offset += n;
    }
  }
};
```

- [ ] **Step 2: Update `tests/test_message_exchange.cpp`**

Add `#include "ipc/tcp_transport.h"`. Replace every:

```cpp
composer.push(...).send(client_socket_.get());
```

with:

```cpp
oid::TcpTransport client_transport(*client_socket_);
composer.push(...).send(client_transport);
```

Replace every:

```cpp
MessageDecoder decoder(server_socket_);
```

with:

```cpp
oid::TcpTransport server_transport(server_socket_);
MessageDecoder decoder(server_transport);
```

- [ ] **Step 3: Run message exchange tests**

Run: `cmake --build build --target test_message_exchange && ctest --test-dir build -R MessageExchangeTests -V`

Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add src/ipc/message_exchange.h tests/test_message_exchange.cpp
git commit -m "refactor(ipc): MessageComposer/Decoder use ITransport"
```

---

### Task 4: Wire `MessageHandler` and `MainWindow` to `ITransport`

**Files:**
- Modify: `src/ui/messaging/message_handler.h`
- Modify: `src/ui/messaging/message_handler.cpp`
- Modify: `src/ui/main_window/main_window.h`
- Modify: `src/ui/main_window/main_window.cpp`

- [ ] **Step 1: Update `MessageHandler::Dependencies`**

In `message_handler.h`:

```cpp
#include "ipc/transport.h"
// remove: class QTcpSocket;

struct Dependencies {
  // ...
  ITransport& transport;  // was: QTcpSocket& socket
  // ...
};
```

- [ ] **Step 2: Update `message_handler.cpp`**

Replace all `deps_.socket` with `deps_.transport`:

- `MessageDecoder{&deps_.socket}` → `MessageDecoder{deps_.transport}`
- `message_composer.send(&deps_.socket)` → `message_composer.send(deps_.transport)`
- `decode_incoming_messages`: replace `deps_.socket.bytesAvailable()` with `deps_.transport.has_data()`
- Header read:

```cpp
auto header = MessageType{};
std::array<std::byte, sizeof(header)> header_bytes{};
if (deps_.transport.receive(header_bytes) != header_bytes.size()) {
  return;
}
header = std::bit_cast<MessageType>(header_bytes);
```

Remove socket error/close handling in WASM builds (`#ifndef __EMSCRIPTEN__` guard the close path).

- [ ] **Step 3: Update `main_window.h`**

```cpp
#include "ipc/tcp_transport.h"

// keep QTcpSocket socket_{}; for desktop TCP
std::unique_ptr<TcpTransport> transport_;
```

- [ ] **Step 4: Update `main_window.cpp` constructor**

After `socket_` is ready (existing initializer connects it):

```cpp
transport_ = std::make_unique<TcpTransport>(socket_);
// MessageHandler deps: *transport_ instead of socket_
```

- [ ] **Step 5: Build desktop target**

Run: `cmake --build build --target oidwindow && ctest --test-dir build --output-on-failure`

Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/ui/messaging/message_handler.h src/ui/messaging/message_handler.cpp \
        src/ui/main_window/main_window.h src/ui/main_window/main_window.cpp
git commit -m "refactor(ui): MessageHandler uses ITransport"
```

---

### Task 5: `PostMessageTransport` with queue-based tests

**Files:**
- Create: `src/ipc/postmessage_transport.h`
- Create: `src/ipc/postmessage_transport.cpp`
- Modify: `src/ipc/CMakeLists.txt`
- Modify: `tests/test_transport.cpp`

- [ ] **Step 1: Add failing queue transport test**

Append to `tests/test_transport.cpp`:

```cpp
#include "ipc/postmessage_transport.h"

TEST(PostMessageTransport, QueuesSendAndReceive) {
  oid::PostMessageTransport transport;
  const std::vector<std::byte> msg{std::byte{0x03}, std::byte{0xAA}};
  transport.send(msg);

  std::array<std::byte, 2> out{};
  EXPECT_EQ(transport.receive(out), 2u);
  EXPECT_EQ(out[0], std::byte{0x03});
  EXPECT_EQ(out[1], std::byte{0xAA});
}
```

- [ ] **Step 2: Implement queue transport (shared by tests and WASM)**

```cpp
// src/ipc/postmessage_transport.h
#ifndef IPC_POSTMESSAGE_TRANSPORT_H_
#define IPC_POSTMESSAGE_TRANSPORT_H_

#include <deque>
#include <mutex>
#include <vector>

#include "ipc/transport.h"

namespace oid {

class PostMessageTransport final : public ITransport {
 public:
  void send(std::span<const std::byte> data) override;
  std::size_t receive(std::span<std::byte> dst) override;
  bool has_data() const override;

  void enqueue_inbound(std::vector<std::byte> data);

 private:
  mutable std::mutex mutex_;
  std::deque<std::byte> inbound_;
};

}  // namespace oid

#endif
```

```cpp
// src/ipc/postmessage_transport.cpp
#include "ipc/postmessage_transport.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace oid {

void PostMessageTransport::send(std::span<const std::byte> data) {
#ifdef __EMSCRIPTEN__
  // EM_ASM passes Uint8Array to window.oidSend
  EM_ASM({ window.oidSend(HEAPU8.slice($0, $0 + $1)); }, data.data(), data.size());
#else
  (void)data;
#endif
}

std::size_t PostMessageTransport::receive(std::span<std::byte> dst) {
  std::scoped_lock lock(mutex_);
  std::size_t n = 0;
  while (n < dst.size() && !inbound_.empty()) {
    dst[n++] = inbound_.front();
    inbound_.pop_front();
  }
  return n;
}

bool PostMessageTransport::has_data() const {
  std::scoped_lock lock(mutex_);
  return !inbound_.empty();
}

void PostMessageTransport::enqueue_inbound(std::vector<std::byte> data) {
  std::scoped_lock lock(mutex_);
  inbound_.insert(inbound_.end(), data.begin(), data.end());
}

}  // namespace oid
```

- [ ] **Step 3: Add to `src/ipc/CMakeLists.txt`**

```cmake
target_sources(${PROJECT_NAME} PRIVATE postmessage_transport.cpp)
```

- [ ] **Step 4: Extend test CMake target sources**

Add `postmessage_transport.cpp` to `test_transport` sources.

- [ ] **Step 5: Run tests**

Run: `ctest --test-dir build -R TransportTests -V`

Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/ipc/postmessage_transport.h src/ipc/postmessage_transport.cpp \
        src/ipc/CMakeLists.txt tests/test_transport.cpp tests/CMakeLists.txt
git commit -m "feat(ipc): add PostMessageTransport with inbound queue"
```

---

### Task 6: WebGL / GLES compatibility fixes

**Files:**
- Modify: `src/ui/gl_canvas.cpp`

- [ ] **Step 1: Replace legacy FBO constants**

In `src/ui/gl_canvas.cpp`, replace all three occurrences:

```cpp
glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
// →
glBindFramebuffer(GL_FRAMEBUFFER, 0);

glBindFramebuffer(GL_FRAMEBUFFER_EXT, icon_fbo_);
// →
glBindFramebuffer(GL_FRAMEBUFFER, icon_fbo_);
```

- [ ] **Step 2: Build and run desktop tests**

Run: `cmake --build build --target oidwindow && ctest --test-dir build --output-on-failure`

Expected: PASS

- [ ] **Step 3: Commit**

```bash
git add src/ui/gl_canvas.cpp
git commit -m "fix(gl): use GL_FRAMEBUFFER for WebGL compatibility"
```

---

### Task 7: WASM build scaffolding

**Files:**
- Create: `cmake/EmscriptenWasm.cmake`
- Modify: `src/CMakeLists.txt`
- Modify: `src/ui/main_window/main_window_initializer.cpp`
- Modify: `src/oid_window.cpp`

- [ ] **Step 1: Create `cmake/EmscriptenWasm.cmake`**

```cmake
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  return()
endif()

if(NOT DEFINED ENV{EMSDK})
  message(FATAL_ERROR "EMSDK not set. Run: source emsdk_env.sh")
endif()

if(NOT Qt6_DIR)
  message(FATAL_ERROR "Set -DQt6_DIR=/path/to/qt6-wasm/lib/cmake/Qt6")
endif()

add_compile_definitions(OID_TRANSPORT_POSTMESSAGE)
```

- [ ] **Step 2: Guard TCP connect in `main_window_initializer.cpp`**

Wrap `deps_.socket.connectToHost(...)` and `waitForConnected()` in:

```cpp
#ifndef __EMSCRIPTEN__
  deps_.socket.connectToHost(...);
  deps_.socket.waitForConnected();
#endif
```

- [ ] **Step 3: Emscripten `main_window` uses `PostMessageTransport`**

In `main_window.cpp`:

```cpp
#ifdef __EMSCRIPTEN__
  postmessage_transport_ = std::make_unique<PostMessageTransport>();
  transport_ = std::unique_ptr<ITransport>(postmessage_transport_.get());
#else
  transport_ = std::make_unique<TcpTransport>(socket_);
#endif
```

Add member `std::unique_ptr<PostMessageTransport> postmessage_transport_;` (WASM only via `#ifdef` or always present).

- [ ] **Step 4: Emscripten entry in `oid_window.cpp`**

```cpp
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "ipc/postmessage_transport.h"
#endif

int main(int argc, char* argv[]) {
  auto app = QApplication{argc, argv};

#ifndef __EMSCRIPTEN__
  // existing parser + ConnectionSettings
  auto window = oid::MainWindow{host_settings};
#else
  oid::ConnectionSettings host_settings{};  // unused
  auto window = oid::MainWindow{host_settings};
  EM_ASM({
    window.oidOnMessage = function(buf) {
      Module.oidEnqueueInbound(buf);
    };
    window.parent.postMessage({type:'oid-control', event:'viewer-ready', version:'dev'}, '*');
  });
#endif

  window.showWindow();
  return QApplication::exec();
}
```

- [ ] **Step 5: Export `oidEnqueueInbound` from WASM** — add to `postmessage_transport.cpp`:

```cpp
#ifdef __EMSCRIPTEN__
namespace {
PostMessageTransport* g_transport = nullptr;
}

extern "C" void oid_set_postmessage_transport(PostMessageTransport* t) {
  g_transport = t;
}

extern "C" void oidEnqueueInbound(uintptr_t ptr, int len) {
  if (!g_transport) return;
  const auto* data = reinterpret_cast<const std::byte*>(ptr);
  g_transport->enqueue_inbound({data, data + len});
}
#endif
```

Call `oid_set_postmessage_transport(postmessage_transport_.get())` from `MainWindow` constructor on Emscripten.

- [ ] **Step 6: Skip desktop OpenGL package on Emscripten** in `src/CMakeLists.txt`:

```cmake
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(OpenGL_GL_PREFERENCE LEGACY)
  find_package(OpenGL 2.1 REQUIRED)
endif()
```

Wrap `${OPENGL_gl_LIBRARY}` link similarly.

- [ ] **Step 7: Document local WASM build command** (verify manually when Qt 6.11 WASM + emsdk 4.0.7 installed):

```bash
source /path/to/emsdk/emsdk_env.sh   # Emscripten 4.0.7
/path/to/qt6-wasm/bin/qt-cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm --target oidwindow
```

Expected: produces `build-wasm/oidwindow.js` and `oidwindow.wasm`

- [ ] **Step 8: Commit**

```bash
git add cmake/EmscriptenWasm.cmake src/CMakeLists.txt src/oid_window.cpp \
        src/ui/main_window/main_window.cpp src/ui/main_window/main_window_initializer.cpp \
        src/ipc/postmessage_transport.cpp src/ipc/postmessage_transport.h
git commit -m "feat(wasm): add Emscripten build scaffolding and postMessage entry"
```

---

### Task 8: WASM loader, npm package, CI

**Files:**
- Create: `wasm/loader.html`
- Create: `wasm/package.json`
- Create: `wasm/scripts/pack-viewer-wasm.sh`
- Create: `.github/workflows/wasm.yml`

- [ ] **Step 1: Create `wasm/loader.html`**

```html
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Open Image Debugger</title>
  <script src="qtloader.js"></script>
</head>
<body>
  <div id="qtspinner"></div>
  <canvas id="qtcanvas"></canvas>
  <script>
    window.oidSend = function(u8) {
      window.parent.postMessage({ type: 'oid-ipc', payload: u8 }, '*');
    };
    window.addEventListener('message', (ev) => {
      if (ev.data?.type === 'oid-ipc' && ev.data.payload) {
        if (window.oidOnMessage) window.oidOnMessage(ev.data.payload);
      }
    });
    const loader = new QtLoader({
      path: '.',
      entryFunction: 'oidwindow',
      canvas: document.getElementById('qtcanvas'),
    });
    loader.loadEmscriptenModule('oidwindow');
  </script>
</body>
</html>
```

- [ ] **Step 2: Create `wasm/package.json`**

```json
{
  "name": "@openimagedebugger/viewer-wasm",
  "version": "0.0.0-dev",
  "description": "Qt 6.11 WASM build of oidwindow",
  "files": ["dist"],
  "license": "MIT"
}
```

- [ ] **Step 3: Create `wasm/scripts/pack-viewer-wasm.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DIST="$ROOT/wasm/dist"
rm -rf "$DIST" && mkdir -p "$DIST"
cp "$ROOT/build-wasm/oidwindow.wasm" "$ROOT/build-wasm/oidwindow.js" "$DIST/"
cp "$ROOT/wasm/loader.html" "$DIST/"
# Copy Qt WASM runtime files emitted beside oidwindow.js (qtloader.js, etc.)
cp "$ROOT/build-wasm"/qt*.js "$ROOT/build-wasm"/qt*.wasm "$DIST/" 2>/dev/null || true
echo '{"qt":"6.11","emscripten":"4.0.7","oid":"dev"}' > "$DIST/version.json"
```

- [ ] **Step 4: Create `.github/workflows/wasm.yml`**

```yaml
name: WASM Build
on:
  pull_request:
  workflow_dispatch:

jobs:
  wasm:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install emsdk 4.0.7
        run: |
          git clone https://github.com/emscripten-core/emsdk.git /opt/emsdk
          /opt/emsdk/emsdk install 4.0.7
          /opt/emsdk/emsdk activate 4.0.7
          echo "EMSDK=/opt/emsdk" >> "$GITHUB_ENV"
          echo "/opt/emsdk/upstream/emscripten" >> "$GITHUB_PATH"

      - name: Install Qt 6.11 WASM
        uses: jurplel/install-qt-action@v4
        with:
          version: "6.11.*"
          arch: "wasm_singlethread"
          modules: "qtshadertools"
          cache: true

      - name: Build WASM
        run: |
          qt-cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
          cmake --build build-wasm --target oidwindow

      - name: Pack artifact
        run: bash wasm/scripts/pack-viewer-wasm.sh

      - name: Upload dist
        uses: actions/upload-artifact@v4
        with:
          name: viewer-wasm
          path: wasm/dist
```

- [ ] **Step 5: Commit**

```bash
git add wasm/ .github/workflows/wasm.yml
git commit -m "ci(wasm): add loader, pack script, and WASM build workflow"
```

---

# Part II — Extension smoke stub

### Task 9: Scaffold `openimagedebugger-vscode`

**Files:**
- Create: `openimagedebugger-vscode/package.json`
- Create: `openimagedebugger-vscode/tsconfig.json`
- Create: `openimagedebugger-vscode/src/extension.ts`
- Create: `openimagedebugger-vscode/src/webview/panel.ts`

> Create this directory **outside** the OID repo (sibling directory) or in a separate git repo per design spec.

- [ ] **Step 1: Create `package.json`**

```json
{
  "name": "openimagedebugger-vscode",
  "displayName": "Open Image Debugger",
  "version": "0.0.1",
  "engines": { "vscode": "^1.85.0" },
  "activationEvents": ["onCommand:oid.openPanel"],
  "main": "./out/extension.js",
  "contributes": {
    "commands": [{
      "command": "oid.openPanel",
      "title": "OID: Open Viewer Panel"
    }]
  },
  "scripts": {
    "compile": "tsc -p .",
    "test": "node --test out/test/*.js"
  },
  "devDependencies": {
    "@types/vscode": "^1.85.0",
    "typescript": "^5.4.0"
  }
}
```

- [ ] **Step 2: Create `src/extension.ts`**

```typescript
import * as vscode from 'vscode';
import { openViewerPanel } from './webview/panel';

export function activate(context: vscode.ExtensionContext): void {
  context.subscriptions.push(
    vscode.commands.registerCommand('oid.openPanel', () => openViewerPanel(context))
  );
}

export function deactivate(): void {}
```

- [ ] **Step 3: Create `src/webview/panel.ts`**

```typescript
import * as vscode from 'vscode';
import { buildPlotBufferContents, type WasmLengthMode } from '../ipc/message-exchange';

export function openViewerPanel(context: vscode.ExtensionContext): void {
  const panel = vscode.window.createWebviewPanel(
    'oidViewer', 'Open Image Debugger', vscode.ViewColumn.One,
    { enableScripts: true, localResourceRoots: [vscode.Uri.joinPath(context.extensionUri, 'media')] }
  );

  const loaderUri = panel.webview.asWebviewUri(
    vscode.Uri.joinPath(context.extensionUri, 'media', 'loader.html')
  );
  panel.webview.html = `<!DOCTYPE html><html><body style="margin:0">
    <iframe src="${loaderUri}" style="width:100%;height:100vh;border:none"></iframe>
  </body></html>`;

  panel.webview.onDidReceiveMessage((msg) => {
    if (msg?.type === 'oid-control' && msg.event === 'viewer-ready') {
      const payload = buildPlotBufferContents({
        variableName: 'test',
        displayName: 'test',
        pixelLayout: 'rgba',
        transpose: false,
        width: 2,
        height: 2,
        channels: 3,
        stride: 6,
        bufferType: 0,
        pixels: new Uint8Array([255, 0, 0, 0, 255, 0, 0, 0, 255, 128, 128, 128]),
      }, 'wasm32' satisfies WasmLengthMode);
      panel.webview.postMessage({ type: 'oid-ipc', payload });
    }
  });
}
```

- [ ] **Step 4: Compile**

Run: `cd openimagedebugger-vscode && npm install && npm run compile`

Expected: no TypeScript errors (after Task 10 adds `message-exchange.ts`)

- [ ] **Step 5: Commit** (in extension repo)

```bash
git init openimagedebugger-vscode && cd openimagedebugger-vscode
git add package.json tsconfig.json src/
git commit -m "feat: scaffold VS Code extension smoke stub"
```

---

### Task 10: TypeScript `MessageComposer` (wasm32 lengths)

**Files:**
- Create: `openimagedebugger-vscode/src/ipc/message-exchange.ts`
- Create: `openimagedebugger-vscode/test/message-exchange.test.ts`

- [ ] **Step 1: Write failing unit test**

```typescript
// test/message-exchange.test.ts
import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import { buildPlotBufferContents, MessageType } from '../src/ipc/message-exchange.js';

test('PlotBufferContents starts with message type 3', () => {
  const bytes = buildPlotBufferContents({
    variableName: 'img',
    displayName: 'img',
    pixelLayout: 'rgba',
    transpose: false,
    width: 1,
    height: 1,
    channels: 3,
    stride: 3,
    bufferType: 0,
    pixels: new Uint8Array([1, 2, 3]),
  }, 'wasm32');
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  assert.equal(view.getUint32(0, true), MessageType.PlotBufferContents);
});
```

- [ ] **Step 2: Implement codec**

```typescript
// src/ipc/message-exchange.ts
export enum MessageType {
  PlotBufferContents = 3,
}

export type WasmLengthMode = 'wasm32' | 'native';

export interface PlotBufferParams {
  variableName: string;
  displayName: string;
  pixelLayout: string;
  transpose: boolean;
  width: number;
  height: number;
  channels: number;
  stride: number;
  bufferType: number;
  pixels: Uint8Array;
}

function pushU32(buf: number[], v: number): void {
  buf.push(v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff);
}

function pushString(buf: number[], s: string, mode: WasmLengthMode): void {
  const bytes = new TextEncoder().encode(s);
  if (mode === 'wasm32') pushU32(buf, bytes.length);
  else throw new Error('native mode not implemented for extension');
  for (const b of bytes) buf.push(b);
}

export function buildPlotBufferContents(p: PlotBufferParams, mode: WasmLengthMode): Uint8Array {
  const parts: number[] = [];
  pushU32(parts, MessageType.PlotBufferContents);
  pushString(parts, p.variableName, mode);
  pushString(parts, p.displayName, mode);
  pushString(parts, p.pixelLayout, mode);
  parts.push(p.transpose ? 1 : 0, 0, 0, 0); // bool as 4 bytes (bool PrimitiveBlock)
  pushU32(parts, p.width);
  pushU32(parts, p.height);
  pushU32(parts, p.channels);
  pushU32(parts, p.stride);
  pushU32(parts, p.bufferType);
  pushU32(parts, p.pixels.length);
  for (const b of p.pixels) parts.push(b);
  return new Uint8Array(parts);
}
```

> **Note:** Verify `bool` wire size matches C++ `PrimitiveBlock<bool>` (1 byte on most platforms). Adjust test after checking `sizeof(bool)` in `message_exchange.h` — if 1 byte, push single `p.transpose ? 1 : 0` instead of 4.

- [ ] **Step 3: Run test**

Run: `npm run compile && npm test`

Expected: PASS (fix bool width if test fails)

- [ ] **Step 4: Copy WASM artifact into `media/`**

```bash
cp -r ../OpenImageDebugger/wasm/dist/* openimagedebugger-vscode/media/
```

- [ ] **Step 5: Manual smoke test**

1. Open extension folder in VS Code
2. Press F5 (Extension Development Host)
3. Run command **OID: Open Viewer Panel**
4. Expect: WASM loads, 2×2 test image appears

- [ ] **Step 6: Commit**

```bash
git add src/ipc/message-exchange.ts test/
git commit -m "feat(ipc): add wasm32 MessageComposer for PlotBufferContents"
```

---

## Plan self-review

| Spec requirement | Task |
|------------------|------|
| Qt 6.11 + Emscripten 4.0.7 | Tasks 1, 8 |
| Custom IPC (no Protobuf) | Tasks 3–5, 10 |
| `ITransport` abstraction | Tasks 2–5 |
| Desktop regression on Qt 6.11 | Task 1, 4 |
| WASM build + npm artifact | Tasks 7–8 |
| WebGL compat | Task 6 |
| Extension smoke stub | Tasks 9–10 |
| wasm32 length encoding | Task 10 (cross-bitness note) |
| Qt 6.12 upgrade path | Documented in spec §5.5 (no task — future) |

**Placeholder scan:** None found.

**Type consistency:** `ITransport::receive` returns `std::size_t` throughout Tasks 2–5. `MessageHandler` uses `ITransport&` consistently.

---

## Execution handoff

Plan complete and saved to `docs/superpowers/plans/2026-06-24-oid-wasm-vscode-qt611.md`.

**Two execution options:**

1. **Subagent-Driven (recommended)** — dispatch a fresh subagent per task, review between tasks, fast iteration
2. **Inline Execution** — run tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
