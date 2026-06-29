# Platform `#ifdef` Consolidation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove scattered `#ifdef __EMSCRIPTEN__` from the codebase by routing all WASM/native divergence through a single `src/platform/` boundary, using compile-time separation (CMake-selected translation units + injected strategy objects).

**Architecture:** A new `src/platform/` directory owns every platform branch. Tangled shared+divergent logic is factored behind small interfaces/free functions (`ITransport` factory, `RenderScheduler`, `GlDialect`, `app_platform`, `session_persistence`, `ui_dialogs`) wired at one composition point. Wholly-disjoint method bodies (`GLCanvas`) are split into CMake-selected `_native.cpp`/`_wasm.cpp` translation units. After the work, `#ifdef __EMSCRIPTEN__` survives only inside `src/platform/` and in the single `GLCanvas` base-class line in `gl_canvas.h`.

**Tech Stack:** C++20, Qt6 (6.11), CMake (≥3.28), Emscripten/Qt-for-WebAssembly, GoogleTest.

## Global Constraints

- Pure restructuring: **no behavior change**. Moved bodies must be logic-equivalent to the originals.
- The MIT license header block (lines 1-24 of every existing source file) must be copied verbatim to every new file.
- All new code lives in `namespace oid` (or `namespace oid::platform` for platform helpers), matching existing style: 4-space indent, trailing-`_` private members, `[[nodiscard]]` on pure getters.
- Transport interface type is `oid::ITransport` (in `src/ipc/transport.h`).
- The only permitted `#ifdef __EMSCRIPTEN__` outside `src/platform/` after this work is the `GLCanvas` base-class block in `src/ui/gl_canvas.h`.
- Do NOT touch unrelated guards: `Q_OS_DARWIN` (`event_handler.cpp:78`), Python-version guards (`python_native_interface.*`), thirdparty.

### Per-task verification cycle

Every task ends with this cycle (substitute your build dir if not `cmake-build-debug`):

```bash
# 1. Native configure + build (always required)
cmake -S . -B cmake-build-debug -DBUILD_TESTS=ON >/dev/null
cmake --build cmake-build-debug --target oidwindow -j 4

# 2. Native tests (always required)
ctest --test-dir cmake-build-debug --output-on-failure

# 3. WASM build (required IF EMSDK is set up; otherwise note "skipped: no EMSDK")
#    Needs: source $EMSDK/emsdk_env.sh ; Qt6 wasm kit at $QT6_WASM_DIR
emcmake cmake -S . -B build-wasm -DQt6_DIR="$QT6_WASM_DIR/lib/cmake/Qt6" >/dev/null
cmake --build build-wasm --target oidwindow -j 4
```

`Expected:` both builds succeed; ctest reports all pass. If EMSDK is unavailable, the WASM
step is skipped and the task is marked "WASM-build-unverified" for a later pass — but the
code MUST still be written so it would compile under Emscripten.

---

## File Structure

**New (`src/platform/`):**

| File | Responsibility |
|------|----------------|
| `platform_config.h` | `constexpr bool kIsWasm` |
| `transport_factory.h` / `_native.cpp` / `_wasm.cpp` | build the right `ITransport` |
| `render_scheduler.h` / `_native.cpp` / `_wasm.cpp` | run-now vs defer-to-GL-queue |
| `gl_dialect.h` / `_native.cpp` / `_wasm.cpp` | shader version, texel/icon formats, GL caps |
| `app_platform.h` / `_native.cpp` / `_wasm.cpp` | surface format, CLI parse, JS hooks, viewer-ready, disconnect guard |
| `session_persistence.h` / `_native.cpp` / `_wasm.cpp` | persist + load settings |
| `ui_dialogs.h` / `_native.cpp` / `_wasm.cpp` | export + context menu + async wiring |

**New (`src/ui/`):** `gl_canvas_native.cpp`, `gl_canvas_wasm.cpp`.

**Modified:** `src/CMakeLists.txt`, `src/oid_window.cpp`, `src/ui/gl_canvas.{h,cpp}`, `src/ui/messaging/message_handler.{h,cpp}`, `src/ui/main_window/main_window.{h,cpp}`, `src/ui/main_window/main_window_initializer.cpp`, `src/ui/events/event_handler.{h,cpp}`, `src/visualization/shader.cpp`, `src/ui/gl_text_renderer.cpp`, `src/visualization/components/buffer.cpp`.

---

## Task 1: Platform scaffold + config + CMake wiring

**Files:**
- Create: `src/platform/platform_config.h`
- Modify: `src/CMakeLists.txt` (the `SOURCES` list, ~line 45-80)

**Interfaces:**
- Produces: `oid::platform::kIsWasm` (`constexpr bool`).

- [ ] **Step 1: Create `src/platform/platform_config.h`**

```cpp
// <MIT header lines 1-24 copied verbatim>
#ifndef PLATFORM_PLATFORM_CONFIG_H_
#define PLATFORM_PLATFORM_CONFIG_H_

namespace oid::platform {

#ifdef __EMSCRIPTEN__
inline constexpr bool kIsWasm = true;
#else
inline constexpr bool kIsWasm = false;
#endif

} // namespace oid::platform

#endif // PLATFORM_PLATFORM_CONFIG_H_
```

- [ ] **Step 2: Add the platform-selected source list to `src/CMakeLists.txt`**

Immediately after the closing `)` of `set(SOURCES ...)` (currently line 80), insert:

```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  list(APPEND SOURCES
    platform/transport_factory_wasm.cpp
    platform/render_scheduler_wasm.cpp
    platform/gl_dialect_wasm.cpp
    platform/app_platform_wasm.cpp
    platform/session_persistence_wasm.cpp
    platform/ui_dialogs_wasm.cpp
    ui/gl_canvas_wasm.cpp)
else()
  list(APPEND SOURCES
    platform/transport_factory_native.cpp
    platform/render_scheduler_native.cpp
    platform/gl_dialect_native.cpp
    platform/app_platform_native.cpp
    platform/session_persistence_native.cpp
    platform/ui_dialogs_native.cpp
    ui/gl_canvas_native.cpp)
endif()
```

> NOTE: the listed `.cpp` files do not exist yet. Each later task creates its pair. To keep the build green between tasks, create each new file referenced by CMake **before** building. Task 1's build step therefore also creates empty-but-valid stubs (below) for every file in the list, which later tasks fill in.

- [ ] **Step 3: Create compiling stubs for every platform `.cpp` in the list**

For each of the 12 platform `.cpp` files and the 2 `gl_canvas_*` files, create a file containing only the MIT header plus `#include "platform/platform_config.h"` (and nothing else). Example `src/platform/transport_factory_native.cpp`:

```cpp
// <MIT header lines 1-24>
#include "platform/platform_config.h"
// Implemented in Task 2.
```

Create the analogous stub for: `transport_factory_wasm.cpp`, `render_scheduler_native.cpp`, `render_scheduler_wasm.cpp`, `gl_dialect_native.cpp`, `gl_dialect_wasm.cpp`, `app_platform_native.cpp`, `app_platform_wasm.cpp`, `session_persistence_native.cpp`, `session_persistence_wasm.cpp`, `ui_dialogs_native.cpp`, `ui_dialogs_wasm.cpp`. For `src/ui/gl_canvas_native.cpp` and `src/ui/gl_canvas_wasm.cpp` use `#include "ui/gl_canvas.h"` instead.

- [ ] **Step 4: Run the verification cycle**

Run the native configure+build+ctest from "Per-task verification cycle".
`Expected:` configures (CMake sees the new sources), builds `oidwindow`, all tests pass. The stubs add no symbols, so nothing breaks.

- [ ] **Step 5: Commit**

```bash
git add src/platform/platform_config.h src/CMakeLists.txt src/platform/*.cpp src/ui/gl_canvas_native.cpp src/ui/gl_canvas_wasm.cpp
git commit -m "build(platform): scaffold src/platform boundary and CMake wiring"
```

---

## Task 2: Transport factory

**Files:**
- Create: `src/platform/transport_factory.h`, fill `src/platform/transport_factory_native.cpp`, `src/platform/transport_factory_wasm.cpp`
- Modify: `src/ui/main_window/main_window.cpp` (158-191, 362-367), `src/ui/main_window/main_window.h` (transport members)

**Interfaces:**
- Consumes: `oid::ITransport`, `oid::TcpTransport`, `oid::PostMessageTransport`.
- Produces:
  - `std::unique_ptr<ITransport> oid::platform::make_transport(const TransportDeps&)`
  - `bool oid::platform::should_quit_on_disconnect(QTcpSocket& socket)`
  - `struct TransportDeps { QTcpSocket& socket; }`

- [ ] **Step 1: Create `src/platform/transport_factory.h`**

```cpp
// <MIT header>
#ifndef PLATFORM_TRANSPORT_FACTORY_H_
#define PLATFORM_TRANSPORT_FACTORY_H_

#include <memory>

class QTcpSocket;

namespace oid {
class ITransport;
namespace platform {

struct TransportDeps {
    QTcpSocket& socket; // ignored on WASM, used by the native TCP transport
};

std::unique_ptr<ITransport> make_transport(const TransportDeps& deps);

// True only on native when the TCP socket has dropped; always false on WASM.
bool should_quit_on_disconnect(QTcpSocket& socket);

} // namespace platform
} // namespace oid

#endif // PLATFORM_TRANSPORT_FACTORY_H_
```

- [ ] **Step 2: Fill `src/platform/transport_factory_native.cpp`**

```cpp
// <MIT header>
#include "platform/transport_factory.h"

#include <QTcpSocket>

#include "ipc/tcp_transport.h"

namespace oid::platform {

std::unique_ptr<ITransport> make_transport(const TransportDeps& deps) {
    return std::make_unique<TcpTransport>(deps.socket);
}

bool should_quit_on_disconnect(QTcpSocket& socket) {
    return socket.state() == QAbstractSocket::UnconnectedState;
}

} // namespace oid::platform
```

- [ ] **Step 3: Fill `src/platform/transport_factory_wasm.cpp`**

```cpp
// <MIT header>
#include "platform/transport_factory.h"

#include "ipc/postmessage_transport.h"

namespace oid::platform {

std::unique_ptr<ITransport> make_transport(const TransportDeps& /*deps*/) {
    auto transport = std::make_unique<PostMessageTransport>();
    oid_set_postmessage_transport(transport.get());
    return transport;
}

bool should_quit_on_disconnect(QTcpSocket& /*socket*/) {
    return false;
}

} // namespace oid::platform
```

- [ ] **Step 4: Replace the transport ifdef in `main_window.cpp`**

In `MainWindow`'s constructor, replace the whole `#ifdef __EMSCRIPTEN__ ... #endif` block (currently 158-191) with:

```cpp
    // Initialize message handler
    transport_ = platform::make_transport({socket_});
    message_handler_ = std::make_unique<MessageHandler>(
        MessageHandler::Dependencies{
            ui_mutex_,
            buffer_data_,
            state_,
            ui_components_,
            *transport_,
            [] { return get_icon_size(); },
            [this] { return std::make_shared<Stage>(*this); },
            [this](const std::shared_ptr<Stage>& stage) {
                set_currently_selected_stage(stage);
            },
            settings_applier_.get()},
        this);
```

Add `#include "platform/transport_factory.h"` to the includes. In `loop()`, replace the native-only disconnect block (362-367) with:

```cpp
    if (platform::should_quit_on_disconnect(socket_)) [[unlikely]] {
        QApplication::quit();
        return;
    }
```

- [ ] **Step 5: Update `main_window.h` transport members**

Find the two platform-specific members (`std::unique_ptr<PostMessageTransport> postmessage_transport_;` and `std::unique_ptr<TcpTransport> tcp_transport_;`, each likely ifdef-guarded) and replace both with a single unguarded member:

```cpp
    std::unique_ptr<ITransport> transport_;
```

Add `#include "ipc/transport.h"` and remove now-unused `postmessage_transport.h` / `tcp_transport.h` includes from the header if present (keep them in the `.cpp` only where needed). `socket_` stays as-is (still used by the native factory and held by `MainWindow`).

- [ ] **Step 6: Run the verification cycle** (native + WASM + ctest). `Expected:` both build; tests pass.

- [ ] **Step 7: Commit**

```bash
git add src/platform/transport_factory.* src/ui/main_window/main_window.cpp src/ui/main_window/main_window.h
git commit -m "refactor(platform): select transport via make_transport factory"
```

---

## Task 3: Render scheduler

**Files:**
- Create: `src/platform/render_scheduler.h`, fill `_native.cpp`, `_wasm.cpp`
- Modify: `src/ui/messaging/message_handler.{h,cpp}`, `src/ui/main_window/main_window.cpp` (scheduler construction + wiring into `MessageHandler::Dependencies`)

**Interfaces:**
- Consumes: `oid::GLCanvas` (for the WASM scheduler).
- Produces:
  - `struct oid::RenderScheduler { virtual ~; virtual void run_gl(std::function<void()>); virtual void run_icon_gl(std::function<void()>); }`
  - `std::unique_ptr<RenderScheduler> oid::platform::make_render_scheduler(GLCanvas& canvas)`
  - `MessageHandler::Dependencies` gains `RenderScheduler& scheduler;` (inserted after `transport`).

- [ ] **Step 1: Create `src/platform/render_scheduler.h`**

```cpp
// <MIT header>
#ifndef PLATFORM_RENDER_SCHEDULER_H_
#define PLATFORM_RENDER_SCHEDULER_H_

#include <functional>
#include <memory>

namespace oid {
class GLCanvas;

class RenderScheduler {
  public:
    virtual ~RenderScheduler() = default;
    // Runs GL work that must execute on the GL thread/context: immediately on
    // native, deferred to the QRhi render callback on WASM.
    virtual void run_gl(std::function<void()> task) = 0;
    virtual void run_icon_gl(std::function<void()> task) = 0;
};

namespace platform {
std::unique_ptr<RenderScheduler> make_render_scheduler(GLCanvas& canvas);
}
} // namespace oid

#endif // PLATFORM_RENDER_SCHEDULER_H_
```

- [ ] **Step 2: Fill `src/platform/render_scheduler_native.cpp`**

```cpp
// <MIT header>
#include "platform/render_scheduler.h"

namespace oid {
namespace {
class ImmediateScheduler final : public RenderScheduler {
  public:
    void run_gl(std::function<void()> task) override { task(); }
    void run_icon_gl(std::function<void()> task) override { task(); }
};
} // namespace

namespace platform {
std::unique_ptr<RenderScheduler> make_render_scheduler(GLCanvas& /*canvas*/) {
    return std::make_unique<ImmediateScheduler>();
}
} // namespace platform
} // namespace oid
```

- [ ] **Step 3: Fill `src/platform/render_scheduler_wasm.cpp`**

```cpp
// <MIT header>
#include "platform/render_scheduler.h"

#include "ui/gl_canvas.h"

namespace oid {
namespace {
class DeferredScheduler final : public RenderScheduler {
  public:
    explicit DeferredScheduler(GLCanvas& canvas) : canvas_{canvas} {}
    void run_gl(std::function<void()> task) override {
        canvas_.schedule_gl(std::move(task));
    }
    void run_icon_gl(std::function<void()> task) override {
        canvas_.schedule_icon_gl(std::move(task));
    }

  private:
    GLCanvas& canvas_;
};
} // namespace

namespace platform {
std::unique_ptr<RenderScheduler> make_render_scheduler(GLCanvas& canvas) {
    return std::make_unique<DeferredScheduler>(canvas);
}
} // namespace platform
} // namespace oid
```

- [ ] **Step 4: Add `scheduler` to `MessageHandler::Dependencies`**

In `message_handler.h`, insert after the `ITransport& transport;` line (65):

```cpp
        RenderScheduler& scheduler;
```

Add `#include "platform/render_scheduler.h"` to `message_handler.h` (forward-declared `RenderScheduler` is already in that header; the include is for the reference member's completeness at call sites — include it in the `.cpp` if a forward declaration suffices in the header). Use a forward declaration `class RenderScheduler;` in the header and `#include` in the `.cpp`.

- [ ] **Step 5: Replace the 5 scheduling ifdefs in `message_handler.cpp`**

Each site currently reads `#ifdef __EMSCRIPTEN__ <canvas->schedule_*(lambda)> #else <lambda body inline> #endif`. Rewrite each so the lambda is defined once and handed to the scheduler. The two representative rewrites:

Icon paint (112-138 region) — keep the `the_dialect()`-based `QImage` construction (Task 5 supplies it; until then keep the existing RGBA/RGB ifdef untouched and only convert the *scheduling* ifdef):

```cpp
    deps_.scheduler.run_icon_gl(paint_icon);
```

Stage GL init/update (241-311 region) — wrap the existing WASM lambda body and reuse it for both platforms:

```cpp
    deps_.scheduler.run_gl([this, stage, params, variable_name_str,
                            ac_enabled = deps_.state.ac_enabled]() mutable {
        // ... exact body from the former #ifdef __EMSCRIPTEN__ branch ...
    });
```

> The former native `#else` branch (immediate `buffer_update`) is now produced by `ImmediateScheduler` running the same lambda synchronously. Verify the lambda body is logic-equivalent to BOTH former branches: the WASM branch did stage init + map insert + select; the native branch did the same work inline. If the two former branches differ in more than now-vs-later, preserve the union of their effects and note the diff in the commit body.

- [ ] **Step 6: Construct + inject the scheduler in `main_window.cpp`**

Before building `MessageHandler` (the block edited in Task 2), add:

```cpp
    render_scheduler_ =
        platform::make_render_scheduler(*ui_components_.ui->bufferPreview);
```

and add `*render_scheduler_,` to the `Dependencies` aggregate immediately after `*transport_,`. Add member `std::unique_ptr<RenderScheduler> render_scheduler_;` to `main_window.h` and `#include "platform/render_scheduler.h"`.

- [ ] **Step 7: Run the verification cycle.** `Expected:` both build; tests pass.

- [ ] **Step 8: Commit**

```bash
git add src/platform/render_scheduler.* src/ui/messaging/message_handler.h src/ui/messaging/message_handler.cpp src/ui/main_window/main_window.cpp src/ui/main_window/main_window.h
git commit -m "refactor(platform): inject RenderScheduler to drop GL scheduling ifdefs"
```

---

## Task 4: GL backend translation-unit split

**Files:**
- Modify: `src/ui/gl_canvas.h` (declarations only), `src/ui/gl_canvas.cpp` (keep shared, remove platform bodies)
- Fill: `src/ui/gl_canvas_native.cpp`, `src/ui/gl_canvas_wasm.cpp`
- Modify: `src/ui/main_window/main_window.cpp` (`prepare_gl_draw`), `src/ui/main_window/main_window.h`

**Interfaces:**
- Consumes: nothing new.
- Produces: unchanged public `GLCanvas` API. The base-class `#ifdef` in `gl_canvas.h` is the single permitted exception.

- [ ] **Step 1: Add a shared ctor-tail hook to `gl_canvas.h`**

Inside the private section, declare:

```cpp
    void platform_ctor_init(); // defined per-platform in gl_canvas_<platform>.cpp
```

Leave the base-class `#ifdef`, the per-platform override declarations (74-96), and the WASM-only members (152-163) exactly as they are — these declarations are allowed to stay under the one base-class-aligned ifdef.

- [ ] **Step 2: Move the WASM method bodies into `src/ui/gl_canvas_wasm.cpp`**

Cut from `gl_canvas.cpp` the bodies currently inside `#ifdef __EMSCRIPTEN__ ... #else` (the block starting at 165 through just before `#else` at 299): `render_width`, `render_height`, `init_gl_state`, `initialize`, `render`, `releaseResources`, `resizeEvent`, `schedule_gl`, `schedule_icon_gl`, `drain_gl_queue`, `drain_icon_gl_queue`. Paste them into `gl_canvas_wasm.cpp` under:

```cpp
// <MIT header>
#include "ui/gl_canvas.h"

#include <rhi/qrhi.h>

#include "main_window/main_window.h"
#include "ui/gl_text_renderer.h"

namespace oid {
// ... pasted WASM bodies ...

void GLCanvas::platform_ctor_init() {
    setApi(Api::OpenGL);
    connect(this, &QRhiWidget::renderFailed, this, [this] {
        initialized_ = false;
        first_paint_completed_ = false;
    });
}
} // namespace oid
```

- [ ] **Step 3: Move the native method bodies into `src/ui/gl_canvas_native.cpp`**

Cut from `gl_canvas.cpp` the bodies inside the `#else ... #endif` (300-345): native `render_width`, `render_height`, `initializeGL`, `paintGL`, `resizeGL`. Paste into:

```cpp
// <MIT header>
#include "ui/gl_canvas.h"

#include <iostream>

#include "main_window/main_window.h"
#include "ui/gl_text_renderer.h"

namespace oid {
// ... pasted native bodies ...

void GLCanvas::platform_ctor_init() {}
} // namespace oid
```

- [ ] **Step 4: Clean up `gl_canvas.cpp`**

Remove the now-empty `#ifdef __EMSCRIPTEN__ ... #endif` span (165-346) entirely. In the constructor, the base-class init list keeps its `#ifdef` (base type differs); replace the platform-specific ctor *body* statements (the WASM `setApi`/`connect` at 51-57) with a single call `platform_ctor_init();`. Move the `#include <rhi/qrhi.h>` (31-33) out of `gl_canvas.cpp` (it is now only needed in `gl_canvas_wasm.cpp`).

- [ ] **Step 5: Handle `prepare_gl_draw` in `main_window`**

Leave `MainWindow::prepare_gl_draw()` (`main_window.cpp:320-343`) and its declaration in `main_window.h` under their existing `#ifdef __EMSCRIPTEN__`. This is a WASM-only render hook tied to the QRhi lifecycle; keeping it WASM-only avoids dead native code and introduces no scattered ifdef beyond the GL-backend boundary already established here. (Document in the commit that this ifdef pair is intentionally retained as part of the GL-backend type-level boundary.)

- [ ] **Step 6: Run the verification cycle.** `Expected:` native builds using `gl_canvas_native.cpp`; WASM builds using `gl_canvas_wasm.cpp`; tests pass.

- [ ] **Step 7: Commit**

```bash
git add src/ui/gl_canvas.h src/ui/gl_canvas.cpp src/ui/gl_canvas_native.cpp src/ui/gl_canvas_wasm.cpp
git commit -m "refactor(gl): split GLCanvas platform bodies into per-platform TUs"
```

---

## Task 5: GL dialect & capabilities

**Files:**
- Create: `src/platform/gl_dialect.h`, fill `_native.cpp`, `_wasm.cpp`
- Modify: `src/visualization/shader.cpp` (228-252), `src/ui/gl_text_renderer.cpp` (203-215), `src/visualization/components/buffer.cpp` (736-757, 810-813), `src/ui/messaging/message_handler.cpp` (112-126)

**Interfaces:**
- Produces:
  - `struct oid::GlDialect { std::string_view version_directive; std::string_view fragment_preamble; bool uses_out_color; QImage::Format icon_image_format; int icon_bytes_per_pixel; bool has_texture_wrap_r; GLuint texture_internal_format(GLenum tex_type, GLenum tex_format) const; };`
  - `const GlDialect& oid::the_dialect();`

- [ ] **Step 1: Create `src/platform/gl_dialect.h`**

```cpp
// <MIT header>
#ifndef PLATFORM_GL_DIALECT_H_
#define PLATFORM_GL_DIALECT_H_

#include <string_view>

#include <GL/gl.h>
#include <QImage>

namespace oid {

struct GlDialect {
    std::string_view version_directive;
    std::string_view fragment_preamble;
    bool uses_out_color;
    QImage::Format icon_image_format;
    int icon_bytes_per_pixel;
    bool has_texture_wrap_r;
    GLuint texture_internal_format(GLenum tex_type, GLenum tex_format) const;
};

const GlDialect& the_dialect();

} // namespace oid

#endif // PLATFORM_GL_DIALECT_H_
```

- [ ] **Step 2: Fill `src/platform/gl_dialect_native.cpp`**

```cpp
// <MIT header>
#include "platform/gl_dialect.h"

namespace oid {

GLuint GlDialect::texture_internal_format(GLenum /*tex_type*/,
                                          GLenum /*tex_format*/) const {
    return GL_RGBA32F;
}

const GlDialect& the_dialect() {
    static const GlDialect dialect{
        .version_directive = "#version 120\n",
        .fragment_preamble = "",
        .uses_out_color = false,
        .icon_image_format = QImage::Format_RGB888,
        .icon_bytes_per_pixel = 3,
        .has_texture_wrap_r = true,
    };
    return dialect;
}

} // namespace oid
```

- [ ] **Step 3: Fill `src/platform/gl_dialect_wasm.cpp`**

```cpp
// <MIT header>
#include "platform/gl_dialect.h"

namespace oid {

GLuint GlDialect::texture_internal_format(GLenum tex_type,
                                          GLenum tex_format) const {
    if (tex_type == GL_FLOAT) {
        if (tex_format == GL_RED) return GL_R32F;
        if (tex_format == GL_RG) return GL_RG32F;
        if (tex_format == GL_RGB) return GL_RGB32F;
        return GL_RGBA32F;
    }
    if (tex_format == GL_RED) return GL_R8;
    if (tex_format == GL_RG) return GL_RG8;
    if (tex_format == GL_RGB) return GL_RGB8;
    return GL_RGBA8;
}

const GlDialect& the_dialect() {
    static const GlDialect dialect{
        .version_directive = "#version 300 es\n",
        .fragment_preamble = "precision mediump float;\n"
                             "precision mediump int;\n"
                             "out vec4 oid_fragColor;\n",
        .uses_out_color = true,
        .icon_image_format = QImage::Format_RGBA8888,
        .icon_bytes_per_pixel = 4,
        .has_texture_wrap_r = false,
    };
    return dialect;
}

} // namespace oid
```

- [ ] **Step 4: Rewrite `shader.cpp::compile()` to use the dialect**

Replace the `#ifdef __EMSCRIPTEN__ ... #else ... #endif` (228-252) with a single dialect-driven build. The GLES `adapt_shader_source_for_gles` call is gated by `the_dialect().uses_out_color`:

```cpp
    const auto& dialect = the_dialect();
    std::ostringstream full_source;
    full_source << dialect.version_directive << dialect.fragment_preamble;
    if (dialect.uses_out_color && type == GL_FRAGMENT_SHADER) {
        // (the "out vec4 oid_fragColor" decl is already in fragment_preamble)
    }
    const auto body = dialect.uses_out_color
        ? adapt_shader_source_for_gles(type, std::string{source})
        : std::string{source};
    full_source << get_texel_format_define() << get_source_channel_define()
                << "#define PIXEL_LAYOUT " << pixel_layout_ << "\n" << body;
    const auto source_string = full_source.str();
    const auto* src_ptr = source_string.c_str();
    gl_canvas_.glShaderSource(shader, 1, &src_ptr, nullptr);
```

Add `#include "platform/gl_dialect.h"`. Confirm the native shader still receives the same effective text it did before (version 120 + defines + raw source). The former native path passed 6 separate strings; concatenating them into one is behaviorally identical to `glShaderSource`.

- [ ] **Step 5: Apply the dialect in the remaining sites**

- `gl_text_renderer.cpp` (203-215): replace its version `#ifdef` with `the_dialect().version_directive` / `.fragment_preamble`, mirroring Step 4.
- `message_handler.cpp` (112-126): replace the RGBA/RGB `#ifdef` with `the_dialect().icon_bytes_per_pixel` and `the_dialect().icon_image_format`:
  ```cpp
  const auto bytes_per_line = icon_width * the_dialect().icon_bytes_per_pixel;
  const auto bufferIcon = QImage{stage->get_buffer_icon().data(), icon_width,
                                 icon_height, bytes_per_line,
                                 the_dialect().icon_image_format};
  ```
- `buffer.cpp` (736-757): replace the internal-format `#ifdef` with
  `const auto internal_format = the_dialect().texture_internal_format(tex_type, tex_format);`
- `buffer.cpp` (810-813): replace the `#ifndef __EMSCRIPTEN__` WRAP_R guard with
  `if (the_dialect().has_texture_wrap_r) { gl_canvas_ref().glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE); }`

Add `#include "platform/gl_dialect.h"` to each of these three files.

- [ ] **Step 6: Run the verification cycle.** `Expected:` both build; tests pass; rendered output unchanged (no behavior change).

- [ ] **Step 7: Commit**

```bash
git add src/platform/gl_dialect.* src/visualization/shader.cpp src/ui/gl_text_renderer.cpp src/visualization/components/buffer.cpp src/ui/messaging/message_handler.cpp
git commit -m "refactor(gl): route shader/texture format divergence through GlDialect"
```

---

## Task 6: App bootstrap & lifecycle hooks

**Files:**
- Create: `src/platform/app_platform.h`, fill `_native.cpp`, `_wasm.cpp`
- Modify: `src/oid_window.cpp` (28-86), `src/ui/main_window/main_window.cpp` (32-44 includes, 371-378 viewer-ready), `src/ui/main_window/main_window_initializer.cpp` (362, 383)

**Interfaces:**
- Consumes: `oid::ConnectionSettings`, `oid::MainWindow`.
- Produces:
  - `void oid::platform::configure_surface_format();`
  - `oid::ConnectionSettings oid::platform::parse_connection_settings(int argc, char** argv);`
  - `void oid::platform::install_inbound_hooks();`
  - `void oid::platform::notify_viewer_ready_once(bool window_ready, bool first_paint_done);`

- [ ] **Step 1: Create `src/platform/app_platform.h`**

```cpp
// <MIT header>
#ifndef PLATFORM_APP_PLATFORM_H_
#define PLATFORM_APP_PLATFORM_H_

#include "ui/main_window/connection_settings.h" // adjust to actual path of ConnectionSettings

namespace oid::platform {

void configure_surface_format();
ConnectionSettings parse_connection_settings(int argc, char** argv);
void install_inbound_hooks();
// WASM: emits oid_notify_viewer_ready exactly once when both flags are true.
// Native: no-op.
void notify_viewer_ready_once(bool window_ready, bool first_paint_done);

} // namespace oid::platform

#endif // PLATFORM_APP_PLATFORM_H_
```

> Before writing, grep for the real definition of `ConnectionSettings` and use its actual header path in the include.

- [ ] **Step 2: Fill `src/platform/app_platform_native.cpp`**

```cpp
// <MIT header>
#include "platform/app_platform.h"

#include <QCommandLineParser>
#include <QCoreApplication>

namespace oid::platform {

void configure_surface_format() {}

ConnectionSettings parse_connection_settings(int /*argc*/, char** /*argv*/) {
    auto parser = QCommandLineParser{};
    parser.addOptions({
        {"h", "hostname", "hostname", "127.0.0.1"},
        {"p", "port", "port", "9588"},
    });
    parser.parse(QCoreApplication::arguments());
    auto settings = ConnectionSettings{};
    settings.url = parser.value("h").toStdString();
    settings.port = static_cast<uint16_t>(parser.value("p").toUInt());
    return settings;
}

void install_inbound_hooks() {}
void notify_viewer_ready_once(bool, bool) {}

} // namespace oid::platform
```

> `parse_connection_settings` reads `QCoreApplication::arguments()`, so it must be called after `QApplication` is constructed (it already is in `main()`). `argc/argv` are unused but kept for symmetry.

- [ ] **Step 3: Fill `src/platform/app_platform_wasm.cpp`**

Move the `EM_JS(... oid_install_js_hooks ...)` block and `#include <emscripten.h>` here from `oid_window.cpp`. Implement:

```cpp
// <MIT header>
#include "platform/app_platform.h"

#include <QSurfaceFormat>
#include <emscripten.h>

#include "ipc/postmessage_transport.h"

namespace oid {
// EM_JS hook (moved verbatim from oid_window.cpp) installing window.oidOnMessage
EM_JS(void, oid_install_js_hooks, (), { /* ... verbatim ... */ });

namespace platform {

void configure_surface_format() {
    auto format = QSurfaceFormat{};
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(3, 0);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
}

ConnectionSettings parse_connection_settings(int, char**) {
    return ConnectionSettings{};
}

void install_inbound_hooks() { oid_install_js_hooks(); }

void notify_viewer_ready_once(bool window_ready, bool first_paint_done) {
    static bool sent = false;
    if (!sent && window_ready && first_paint_done) {
        oid_notify_viewer_ready();
        sent = true;
    }
}

} // namespace platform
} // namespace oid
```

> Grep for the existing declaration of `oid_notify_viewer_ready` and include its header. If it is declared in `main_window.cpp` only, move its declaration to an appropriate IPC header so both files see it.

- [ ] **Step 4: Rewrite `oid_window.cpp::main()`**

```cpp
// <MIT header + remaining includes>
#include "platform/app_platform.h"

int main(int argc, char* argv[]) {
    oid::platform::configure_surface_format();
    auto app = QApplication{argc, argv};
    const auto settings = oid::platform::parse_connection_settings(argc, argv);
    oid::platform::install_inbound_hooks();
    auto window = oid::MainWindow{settings};
    window.showWindow();
    return QApplication::exec();
}
```

Remove the `#ifdef __EMSCRIPTEN__` includes/EM_JS block (28-54) and both `#ifdef` blocks in the old `main()` (57-83).

- [ ] **Step 5: Replace the viewer-ready ifdef in `main_window.cpp::loop()`**

Replace the WASM-only block (371-378) with:

```cpp
    platform::notify_viewer_ready_once(
        is_window_ready(), gl_canvas_ptr_->has_completed_first_paint());
```

Add `#include "platform/app_platform.h"`. Remove the now-unused `emscripten.h` include and EM_JS from `main_window.cpp` if present (they live in `app_platform_wasm.cpp` now).

- [ ] **Step 6: Fold the `main_window_initializer.cpp` ifdefs (362, 383)**

Read both blocks. If they reference WASM-only symbols, wrap them in a small `platform::` helper added to `app_platform.h`; if both branches type-check on both platforms, replace with `if constexpr (platform::kIsWasm) { ... } else { ... }` (include `platform/platform_config.h`). Pick per-block; document the choice in the commit.

- [ ] **Step 7: Run the verification cycle.** `Expected:` both build; tests pass.

- [ ] **Step 8: Commit**

```bash
git add src/platform/app_platform.* src/oid_window.cpp src/ui/main_window/main_window.cpp src/ui/main_window/main_window_initializer.cpp
git commit -m "refactor(platform): move app bootstrap and lifecycle hooks behind app_platform"
```

---

## Task 7: Session persistence

**Files:**
- Create: `src/platform/session_persistence.h`, fill `_native.cpp`, `_wasm.cpp`
- Modify: `src/ui/main_window/main_window.cpp` (292-294 load, 479-526 persist), `main_window.h` if needed

**Interfaces:**
- Consumes: the existing `SessionStateCallbacks`/`SessionStateExtraInputs`, `SettingsManager`, `serialize_session_state_delta`, `MessageComposer`, `ITransport`, `ChannelNames`, `UIComponents`.
- Produces:
  - `void oid::platform::persist_session(const SessionStateCallbacks& callbacks, const PersistDeps& deps);`
  - `void oid::platform::load_settings_if_local(SettingsManager& mgr);`
  - `struct PersistDeps { ITransport* transport; const ChannelNames& channel_names; UIComponents& ui_components; };`

> Grep for the exact types of `callbacks`, `SessionStateExtraInputs`, `serialize_session_state_delta`, and `MessageType::SessionStateChanged` before writing; use real names/paths.

- [ ] **Step 1: Create `src/platform/session_persistence.h`** with the two declarations and `PersistDeps` struct (real member types from the grep).

- [ ] **Step 2: Fill `_native.cpp`**

```cpp
// <MIT header>
#include "platform/session_persistence.h"
#include "ui/main_window/settings_manager.h"

namespace oid::platform {

void persist_session(const SessionStateCallbacks& callbacks,
                     const PersistDeps& /*deps*/) {
    SettingsManager::persist_settings(callbacks);
}

void load_settings_if_local(SettingsManager& mgr) { mgr.load_settings(); }

} // namespace oid::platform
```

- [ ] **Step 3: Fill `_wasm.cpp`** — move the colorspace/list-position serialization and `SessionStateChanged` send (the body of `main_window.cpp:479-523`) here, reading from `deps.channel_names`, `deps.ui_components`, `deps.transport`. `load_settings_if_local` is a no-op.

- [ ] **Step 4: Replace the call sites in `main_window.cpp`**

- Persistence block (479-526) becomes:
  ```cpp
  platform::persist_session(callbacks,
                            {transport_.get(), channel_names_, ui_components_});
  ```
- Startup load (292-294) becomes:
  ```cpp
  platform::load_settings_if_local(*settings_manager_);
  ```
Add `#include "platform/session_persistence.h"`.

- [ ] **Step 5: Run the verification cycle.** `Expected:` both build; tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/platform/session_persistence.* src/ui/main_window/main_window.cpp
git commit -m "refactor(platform): move session persistence behind platform layer"
```

---

## Task 8: UI event-loop / dialog semantics

**Files:**
- Create: `src/platform/ui_dialogs.h`, fill `_native.cpp`, `_wasm.cpp`
- Modify: `src/ui/events/event_handler.{h,cpp}` (379-465 export, 491-505 menu), `src/ui/main_window/main_window.cpp` (260-273 async wiring)

**Interfaces:**
- Consumes: `oid::Stage`, `oid::Buffer`, `oid::UIEventHandler`, `oid::MessageHandler`, Qt widgets.
- Produces:
  - `void oid::platform::request_buffer_export(const ExportRequest& req);`
  - `void oid::platform::show_buffer_context_menu(QWidget& parent, QPoint global_pos, const QString& buffer_name, UIEventHandler& handler);`
  - `void oid::platform::wire_async_export_signals(UIEventHandler& event_handler, MessageHandler& message_handler);`
  - `struct ExportRequest { ... }` (fields chosen from the two former branches — grep both).

> Read BOTH branches of `export_buffer` (379-465) before designing `ExportRequest`. The native branch opens `QFileDialog`; the WASM branch gathers contrast and emits `exportBufferRequested`. The struct must carry everything both need (stage, buffer name, contrast list, the emitting object).

- [ ] **Step 1: Create `src/platform/ui_dialogs.h`** with the three declarations and `ExportRequest`.

- [ ] **Step 2: Fill `_native.cpp`** — paste the former `#else` body of `export_buffer` (QFileDialog flow) into `request_buffer_export`; paste the stack-`QMenu`+`exec()` body into `show_buffer_context_menu`; `wire_async_export_signals` is a no-op.

- [ ] **Step 3: Fill `_wasm.cpp`** — paste the former `#ifdef` body (gather contrast, `emit exportBufferRequested`) into `request_buffer_export`; paste the heap-`QMenu`+`popup()` body into `show_buffer_context_menu`; `wire_async_export_signals` performs the `connect()` calls from `main_window.cpp:260-273`.

- [ ] **Step 4: Reduce `event_handler.cpp` call sites**

- `export_buffer` (379-465) collapses to a single `platform::request_buffer_export({...});`.
- The context-menu ifdef (491-505) collapses to `platform::show_buffer_context_menu(*ui_components_.ui->imageList, globalPos, buffer_name.toString(), *this);`.
Add `#include "platform/ui_dialogs.h"`.

- [ ] **Step 5: Make the signals unconditional + replace the wiring**

In `event_handler.h` / `message_handler.h`, ensure `exportBufferRequested`, `request_export_buffer`, `exportSelectedBufferRequested`, `export_selected_buffer`, `bufferRemoved` are declared unconditionally (remove any `#ifdef` around them; an unused signal/slot is harmless on native). In `main_window.cpp`, replace the WASM-only connect block (260-273) with:

```cpp
    platform::wire_async_export_signals(*event_handler_, *message_handler_);
```

- [ ] **Step 6: Run the verification cycle.** `Expected:` both build; native export still opens a file dialog; WASM still emits the signal; tests pass.

- [ ] **Step 7: Commit**

```bash
git add src/platform/ui_dialogs.* src/ui/events/event_handler.h src/ui/events/event_handler.cpp src/ui/messaging/message_handler.h src/ui/main_window/main_window.cpp
git commit -m "refactor(platform): move export/context-menu dialog semantics behind ui_dialogs"
```

---

## Task 9: Grep gate & cleanup sweep

**Files:**
- Modify: any file still violating the invariant (expected: none beyond the GLCanvas line)
- Optional create: `scripts/check_platform_ifdefs.sh`

- [ ] **Step 1: Run the grep gate**

```bash
grep -rn "__EMSCRIPTEN__" src --include=*.cpp | grep -v "src/platform/"
```

`Expected:` empty output. If any line remains, route it through the appropriate platform helper from Tasks 2-8 (do not leave it).

```bash
grep -rn "__EMSCRIPTEN__" src --include=*.h | grep -v "src/platform/"
```

`Expected:` exactly two lines — the `GLCanvas` base-class block in `gl_canvas.h` (and its paired override/member declarations count as the same allowed block). Confirm nothing else (e.g. stray transport members) remains.

- [ ] **Step 2: (Optional) Add a CI check script `scripts/check_platform_ifdefs.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail
violations=$(grep -rn "__EMSCRIPTEN__" src --include=*.cpp | grep -v "src/platform/" || true)
if [[ -n "$violations" ]]; then
  echo "Platform ifdefs leaked outside src/platform/:" >&2
  echo "$violations" >&2
  exit 1
fi
echo "OK: no platform ifdefs in .cpp outside src/platform/"
```

`chmod +x scripts/check_platform_ifdefs.sh` and run it; expect "OK".

- [ ] **Step 3: Final full verification**

Run the native build + ctest and (if EMSDK available) the WASM build one last time. `Expected:` all green.

- [ ] **Step 4: Commit**

```bash
git add scripts/check_platform_ifdefs.sh 2>/dev/null || true
git commit -m "chore(platform): enforce platform-ifdef boundary via grep gate" --allow-empty
```

---

## Self-Review Notes (for the implementer)

- **Spec coverage:** Tasks 2-8 map 1:1 onto the spec's 7 design units (Transport→T2, RenderScheduler→T3, GL backend→T4, GL dialect→T5, Bootstrap→T6, Persistence→T7, UI dialogs→T8). Scaffold (T1) and the grep gate (T9) bracket them.
- **Known unknowns to resolve by grepping before writing (do NOT guess):** exact path/fields of `ConnectionSettings`; declaration site of `oid_notify_viewer_ready`; exact types of `SessionStateCallbacks` / `SessionStateExtraInputs` / `serialize_session_state_delta` / `MessageType`; the full field set needed by `ExportRequest`. Each task flags its own grep.
- **Equivalence risk:** the highest-risk move is Task 3 (scheduler) — the former native and WASM branches of the stage-init path must be confirmed logic-equivalent modulo now-vs-later. Diff both former branches before deleting them.
- **Type consistency:** `make_transport` / `make_render_scheduler` / `the_dialect` / `persist_session` / `request_buffer_export` names are used identically in their producing and consuming tasks.
