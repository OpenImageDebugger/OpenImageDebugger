# Platform `#ifdef` Consolidation Design

**Date:** 2026-06-29
**Branch:** feat/wasm-vscode
**Goal:** Eliminate scattered `#ifdef __EMSCRIPTEN__` from the codebase by consolidating
all platform divergence behind a thin, well-named boundary. Compile-time separation is
preferred over runtime polymorphism. The goal is *consolidate, not eliminate*: a single
permitted ifdef remains in one platform-config header (and the `GLCanvas` base-class
declaration), and everything else moves to per-platform translation units or injected
strategy objects.

## Background

The WASM/VS Code port (`feat/wasm-vscode`) introduced `#ifdef __EMSCRIPTEN__` across ~13
non-thirdparty source files. The build already branches on
`CMAKE_SYSTEM_NAME STREQUAL "Emscripten"` in `src/CMakeLists.txt`, which is the enabler for
compile-time separation: CMake can select which translation unit to compile per platform.

The divergence falls into 6 concern-domains:

1. **Transport/IPC** — `PostMessageTransport` (WASM) vs `TcpTransport` (native).
   Files: `main_window.cpp` (transport member + `loop()` socket-disconnect guard),
   `postmessage_transport.*`, `oid_window.cpp`. A `Transport` interface already exists;
   selection is still done with an ifdef.
2. **GL backend & render lifecycle** — `QRhiWidget` + deferred GL queue (WASM) vs
   `QOpenGLWidget` + immediate GL (native). Files: `gl_canvas.{h,cpp}`,
   `message_handler.cpp`, `main_window.cpp` (`prepare_gl_draw()` is a WASM-only render hook).
   This is the only type-level divergence (the base class differs).
3. **GL dialect & capabilities** — GLSL ES 3.00 vs GLSL 1.20, RGBA vs RGB readback,
   per-format texture internal-format selection (WASM) vs fixed `RGBA32F` (native), and
   capability gaps (`GL_TEXTURE_WRAP_R` absent in WebGL). Files: `shader.cpp`,
   `gl_text_renderer.cpp`, `buffer.cpp`, `gl_canvas.cpp`, `message_handler.cpp`.
4. **App bootstrap & lifecycle hooks** — OpenGLES surface format, CLI parsing, JS inbound
   hooks, the WASM-only `oid_notify_viewer_ready()` in `loop()`. Files: `oid_window.cpp`,
   `main_window.cpp` (`loop()`), `main_window_initializer.cpp`.
5. **Persistence** — VS Code session streaming vs `SettingsManager`; on WASM the local
   `load_settings()` is also skipped. File: `main_window.cpp`.
6. **UI event-loop / dialog semantics** — WASM forbids nested event loops, so blocking
   `QFileDialog::exec()` / `QMenu::exec()` become async `popup()` + signal round-trips to the
   extension. Files: `event_handler.cpp` (`export_buffer`, context menu),
   `main_window.cpp` (WASM-only async-export signal/slot wiring).

## Chosen strategy — Hybrid (right tool per domain)

Two alternatives were rejected:

- **A: build-selected translation units everywhere.** Splitting every divergent `.cpp` into
  `_native`/`_wasm` pairs duplicates the *shared* logic interleaved with the divergent bits
  (`message_handler.cpp` especially). Rejected for the duplication it forces.
- **B: a uniform interface for every domain.** Over-engineers the thin cases (a 2-line
  surface-format difference does not warrant an interface). Rejected for needless indirection.

**Hybrid (chosen):** inject strategy objects where shared and divergent logic are tangled
(highest payoff), and split translation units where method bodies are wholly disjoint. Thin
value differences become a selected constants struct or `if constexpr (kIsWasm)`.

### Invariant

After this work, `#ifdef __EMSCRIPTEN__` may appear **only**:
- inside `src/platform/`, and
- in the `GLCanvas` base-class declaration in `gl_canvas.h` (unavoidable: the base type is
  part of the class declaration, and Qt's moc cannot template a `QObject`-derived class).

A grep gate verifies this.

## The platform boundary

New directory `src/platform/`:

```
src/platform/
  platform_config.h              # constexpr bool kIsWasm; shared platform constants
  transport_factory.h            # make_transport(deps) -> std::unique_ptr<Transport>
  transport_factory_native.cpp   #   TcpTransport
  transport_factory_wasm.cpp     #   PostMessageTransport + oid_set_postmessage_transport
  render_scheduler.h             # RenderScheduler interface
  render_scheduler_native.cpp    #   ImmediateScheduler (runs task now)
  render_scheduler_wasm.cpp      #   DeferredScheduler (canvas->schedule_gl)
  gl_dialect.h                   # GlDialect struct (shader + texture + caps) + the_dialect()
  gl_dialect_native.cpp          #   GLSL 1.20, fixed RGBA32F, WRAP_R available
  gl_dialect_wasm.cpp            #   GLSL ES 3.00, per-format internal format, no WRAP_R
  app_platform.h                 # configure_surface_format / parse_connection_settings /
                                 #   install_inbound_hooks / notify_viewer_ready
  app_platform_native.cpp
  app_platform_wasm.cpp
  session_persistence.h          # persist_session / load_settings_if_local
  session_persistence_native.cpp #   SettingsManager::persist_settings + load_settings
  session_persistence_wasm.cpp   #   serialize delta + stream SessionStateChanged; load = no-op
  ui_dialogs.h                   # request_buffer_export / show_buffer_context_menu /
                                 #   wire_async_export_signals
  ui_dialogs_native.cpp          #   QFileDialog::exec / QMenu::exec; wiring = no-op
  ui_dialogs_wasm.cpp            #   emit signals / QMenu::popup; wiring = connect()s
```

`platform_config.h`:

```cpp
namespace oid::platform {
#ifdef __EMSCRIPTEN__
inline constexpr bool kIsWasm = true;
#else
inline constexpr bool kIsWasm = false;
#endif
}
```

`src/CMakeLists.txt` gains a platform-selected source list appended to `SOURCES`:

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

## Domain designs

These are design units, not a 1:1 map of the 6 concern-domains: the GL backend concern is
split into render scheduling (2) and the type-level backend (3) because they use different
techniques (strategy injection vs translation-unit split).

### 1. Transport

`main_window.cpp:158-191` loses its ifdef. The branches differ only in which transport is
constructed plus one `oid_set_postmessage_transport()` call (WASM). Replace with:

```cpp
transport_ = platform::make_transport({socket_});
message_handler_ = std::make_unique<MessageHandler>(
    MessageHandler::Dependencies{..., *transport_, ...}, this);
```

`make_transport` is implemented per platform. The WASM impl performs the
`oid_set_postmessage_transport` wiring internally. `main_window.h` stores a single
`std::unique_ptr<Transport> transport_` rather than the two platform-specific members.

### 2. Render scheduling (highest payoff)

`message_handler.cpp` branches 5× between "schedule GL work on the canvas queue" (WASM) and
"run it now" (native). Introduce:

```cpp
struct RenderScheduler {
    virtual ~RenderScheduler() = default;
    virtual void run_gl(std::function<void()> task) = 0;       // stage GL init/update
    virtual void run_icon_gl(std::function<void()> task) = 0;  // icon paint
};
```

- `ImmediateScheduler::run_gl(t) { t(); }` (native)
- `DeferredScheduler` holds a `GLCanvas&` and forwards to `schedule_gl` / `schedule_icon_gl`.

`MessageHandler::Dependencies` gains a `RenderScheduler&`. Each ifdef site becomes
`deps_.scheduler.run_gl(...)`. The deferred and immediate paths were already structurally
identical (the same lambda body, only "now vs later" differs), so the lambda is written once
— **no logic duplicated**. The RGBA/RGB `QImage` format difference in the icon path
(`message_handler.cpp:112-126`) is handled via `the_dialect()` (domain 3).

### 3. GL backend (type-level)

`gl_canvas.h` keeps the single base-class ifdef. Diverging method bodies move out:

- `gl_canvas_wasm.cpp`: `initialize`, `render`, `releaseResources`, `resizeEvent`,
  `schedule_gl`, `schedule_icon_gl`, `drain_gl_queue`, `drain_icon_gl_queue`,
  `init_gl_state`, the QRhi `render_width/height`.
- `gl_canvas_native.cpp`: `initializeGL`, `paintGL`, `resizeGL`, the native
  `render_width/height`.
- `gl_canvas.cpp` (shared): ctor/dtor common parts, mouse/wheel events,
  `render_buffer_icon`, icon framebuffer create/destroy, accessors.

The header declares each platform's overrides inside the single existing ifdef block
(declarations only, no logic). The ctor body differs (QRhiWidget setApi + renderFailed
connect vs none) — the platform-specific ctor tail moves to a `platform_ctor_init()` helper
defined in each `gl_canvas_<platform>.cpp`, called from the shared ctor.

`MainWindow::prepare_gl_draw()` (`main_window.cpp:320-343`) is a WASM-only render hook driven
by the QRhi lifecycle. Its declaration stays under the one base-class-aligned ifdef in
`main_window.h`; alternatively, since its body type-checks on both platforms, it can be
declared unconditionally and simply left uncalled on native. Decided per-site; default is to
keep it WASM-only to avoid dead native code.

### 4. GL dialect & capabilities

```cpp
struct GlDialect {
    std::string_view version_directive;   // "#version 300 es\n" | "#version 120\n"
    std::string_view fragment_preamble;   // precision + out decl | ""
    bool uses_out_color;                  // gates GLES in/out adaptation
    QImage::Format icon_image_format;     // RGBA8888 | RGB888
    int icon_bytes_per_pixel;             // 4 | 3
    bool has_texture_wrap_r;              // true native, false WebGL
    GLuint texture_internal_format(GLenum tex_type, GLenum tex_format) const;
};
const GlDialect& the_dialect();
```

`shader.cpp::compile()` assembles its source from `the_dialect()` fields; the GLES source
adaptation (`adapt_shader_source_for_gles`) stays callable but is gated by
`dialect.uses_out_color` (a value, not an ifdef). `gl_text_renderer.cpp` reads the same
struct. `buffer.cpp` uses `texture_internal_format()` (native returns `GL_RGBA32F`
unconditionally; WASM does the per-format `R32F/RG32F/...` selection) and guards `WRAP_R`
with `if (dialect.has_texture_wrap_r)`. Where a difference is a pure value with both branches
type-valid on both platforms, `if constexpr (kIsWasm)` is acceptable instead of a struct
field.

### 5. Bootstrap & lifecycle hooks

`oid_window.cpp main()`:

```cpp
platform::configure_surface_format();                              // no-op native, GLES3 wasm
auto settings = platform::parse_connection_settings(argc, argv);   // CLI native, {} wasm
platform::install_inbound_hooks();                                 // no-op native, EM_JS wasm
auto window = oid::MainWindow{settings};
window.showWindow();
```

The `EM_JS` block and `emscripten.h` include move into `app_platform_wasm.cpp`.

`MainWindow::loop()` carries two opposite hooks: a native-only socket-disconnect guard
(domain 1) and a WASM-only viewer-ready notification. Both become platform calls:
`platform::should_quit_on_disconnect(socket_)` (native checks the socket; WASM returns false)
and `platform::notify_viewer_ready_once(...)` (WASM emits `oid_notify_viewer_ready`; native is
a no-op). `main_window_initializer.cpp`'s two small ifdefs fold into a `platform::` helper or
`if constexpr (kIsWasm)`, decided per-site.

### 6. Persistence

`main_window.cpp:479-526` calls
`platform::persist_session(callbacks, {transport_.get(), channel_names_, ui_components_})`.
Native impl calls `SettingsManager::persist_settings`; WASM impl performs the
colorspace/list-position serialization and streams `SessionStateChanged`. The startup
`load_settings()` (skipped on WASM, `main_window.cpp:292-294`) becomes
`platform::load_settings_if_local(*settings_manager_)` — native loads, WASM is a no-op.

### 7. UI event-loop / dialog semantics

WASM has no nested event loops, so blocking dialogs are replaced by async flows. Behind
`ui_dialogs.h`:

- `platform::request_buffer_export(stage, buffer_name, deps)` — native opens
  `QFileDialog::exec()` and exports synchronously; WASM gathers contrast and emits
  `exportBufferRequested` for the extension to handle. `event_handler.cpp::export_buffer`
  reduces to one call.
- `platform::show_buffer_context_menu(parent, global_pos, actions)` — native uses a stack
  `QMenu` + `exec()`; WASM uses a heap `QMenu{WA_DeleteOnClose}` + `popup()`.
- `platform::wire_async_export_signals(event_handler, message_handler)` — native is a no-op;
  WASM performs the `connect()` calls currently at `main_window.cpp:260-273`.

The signals/slots involved (`exportBufferRequested`, `request_export_buffer`, etc.) are
declared unconditionally in their headers so both platforms compile; only the wiring differs.
If any are currently ifdef-guarded in the header, they are made unconditional as part of this
domain (declaring an unused signal is harmless).

## Verification

- **Both builds compile** — native preset and Emscripten/WASM preset. This is the primary
  safety net, since most divergence is now compile-selected.
- **Existing native tests pass** (`tests/`).
- **Behavioral equivalence** — pure restructuring, no behavior change intended. Moved bodies
  are diffed against originals to confirm equivalence (especially the scheduler lambdas and
  the shader-source assembly).
- **Grep gate** — `grep -rn __EMSCRIPTEN__ src --include=*.cpp` outside `src/platform/`
  returns nothing; including headers, the only hit is the `gl_canvas.h` base-class line and
  `platform_config.h`.

## Rollout

Mechanical but broad (~13 edited files, ~20 new small platform files). Land domain-by-domain,
one commit per domain, each independently buildable and reviewable:

1. `src/platform/` scaffold + `platform_config.h` + CMake wiring.
2. Transport factory (incl. `loop()` disconnect guard).
3. Render scheduler.
4. GL backend split (`gl_canvas`, `prepare_gl_draw`).
5. GL dialect & capabilities (`shader.cpp`, `gl_text_renderer.cpp`, `buffer.cpp`).
6. Bootstrap & lifecycle hooks (surface format, CLI, JS hooks, viewer-ready).
7. Persistence (`persist_session`, `load_settings_if_local`).
8. UI event-loop / dialog semantics (`event_handler.cpp`, async-export wiring).
9. Grep gate / cleanup sweep.

## Out of scope

- Non-Emscripten ifdefs (`Q_OS_DARWIN` in `event_handler.cpp:78`, Python-version guards in
  `python_native_interface.*`, thirdparty Eigen). These are unrelated to the WASM/native
  split and are left untouched.
- Any behavior change, feature addition, or unrelated refactoring.
