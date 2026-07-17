/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include <imgui.h>

// See host/glfw_host_backend.cpp for why this is needed on macOS: GLFW
// transitively pulls in the system OpenGL headers, which mark desktop GL
// deprecated in favor of Metal.
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#if !defined(__EMSCRIPTEN__)
#include "host/agent/agent_server.h"
#include "host/agent/native_view_model.h"
#endif
#include "host/cli_options.h"
#include "host/frame_loop.h"
#include "host/glfw_canvas.h"
#include "host/glfw_host_backend.h"
#include "host/imgui_layer.h"
#include "host/io/file_buffer_loader.h"
#include "host/io/file_open_queue.h"
#include "host/ipc/ipc_client.h"
#include "host/settings/app_settings.h"
#include "host/settings/session_buffers.h"
#include "host/settings/settings_saver.h"
#include "host/stage_view.h"
#include "host/ui/buffer_model.h"
#include "host/ui/export_dialog.h"
#include "host/ui/ipc_buffer_model.h"
#include "host/ui/panels/buffer_list_panel.h"
#include "host/ui/panels/contrast_panel.h"
#include "host/ui/panels/goto_panel.h"
#include "host/ui/panels/menu_bar.h"
#include "host/ui/panels/panel_accessors.h"
#include "host/ui/panels/status_bar.h"
#include "host/ui/panels/symbol_search_panel.h"
#include "host/ui/panels/toolbar_panel.h"
#include "host/ui/shortcuts.h"
#include "host/ui/stage_manager.h"
#include "host/ui/svg_icon_cache.h"
#include "host/ui/thumbnail_cache.h"
#include "host/ui/ui_state.h"
#include "io/buffer_export_core.h"
#include "platform/app_services.h"
#include "platform/display_env.h"
#include "platform/transport_factory.h"
#include "visualization/components/buffer.h"
#include "visualization/stage.h"

namespace {

// Canvas-pane size in LOGICAL points -- the same units the native Qt app
// fed its camera: GLCanvas::render_width() there returned width(), i.e.
// device-independent points (as the legacy Qt GLCanvas did; see tag
// legacy-qt, mouse scale factor
// 1), so every zoom threshold (Camera::scale_at's 0.75 zoom-out floor, the
// 100% reference, BufferValues' zoom-dependent overlay) is defined against
// logical size. Shared with GlfwCanvas via a SizeProvider (see main()) so
// GlfwCanvas::render_width()/render_height() report the PANE's logical size
// rather than the whole window's framebuffer size: the status bar's pixel
// unprojection (status_bar.cpp), Camera::scroll_callback's zoom-anchor NDC
// math and Camera::post_initialize's initial projection (camera.cpp), and the
// buffer-list thumbnail icon render's post-render camera restore
// (glfw_canvas_icon.cpp) all read render_width()/render_height(), and the
// mouse positions fed to GlfwCanvas are in the same pane-logical frame.
// (The StageView FBO itself still rasterizes at framebuffer resolution --
// the camera's projection is resolution-independent, so only the units the
// camera/mouse math sees matter for parity.)
//
// Updated in draw_canvas_pane every frame, and seeded to the window's
// initial logical size in main() below so any pre-first-canvas-frame caller
// (e.g. a just-created Stage's Camera::post_initialize(), triggered by
// buffer-list thumbnail sync running before draw_canvas_pane this frame)
// sees a sane nonzero value rather than 0x0.
struct PaneRenderSize {
    int width = 0;
    int height = 0;
};

// Applies `fn` to the selected Stage, or to every buffer's Stage when
// link-views is on (parity with the Qt app, whose linked views share
// pan/zoom/rotation).
template <typename Fn>
void for_each_view_target(const oid::host::UiState& ui,
                          oid::host::StageManager& stages,
                          const oid::host::BufferModel& model,
                          oid::Stage& sel,
                          Fn&& fn) {
    if (ui.link_views()) {
        for (std::size_t i = 0; i < model.size(); ++i) {
            if (oid::Stage* s = stages.stage_for(i); s != nullptr) {
                fn(*s);
            }
        }
    } else {
        fn(sel);
    }
}

// Draws the canvas pane's content: the StageView image plus its
// drag/scroll/key input handling, sized to whatever rect the caller's
// current ImGui child occupies (ImGui::GetContentRegionAvail()). When
// link-views is on, the drag/scroll/key input fans out to every buffer's
// Stage so switching
// buffers shows them synchronized. Rendering and resize stay on the
// selected Stage (only it is displayed).
void draw_canvas_pane(oid::host::GlfwCanvas& canvas,
                      oid::host::StageView& view,
                      oid::Stage& sel,
                      const oid::host::UiState& ui,
                      oid::host::StageManager& stages,
                      const oid::host::BufferModel& model,
                      PaneRenderSize& pane_size) {
    // Render the Stage at the canvas PANE's size, in framebuffer pixels, so
    // the offscreen texture's aspect ratio matches the on-screen rect it is
    // displayed in: ImGui::Image stretches the texture to fill the rect, so
    // any texture:rect aspect mismatch visibly distorts the buffer.
    const ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    if (canvas_size.x < 1.0f || canvas_size.y < 1.0f) {
        // Pane not laid out yet (e.g. the first frame, before the child sizes
        // settle). Skip this frame rather than sizing to a degenerate rect.
        return;
    }
    // One uniform DPI scale for BOTH axes (display pixels are square), so
    // cw:ch == canvas_size aspect exactly. A per-axis DisplayFramebufferScale
    // that is briefly non-uniform or unset at startup must never skew it.
    float dpi = ImGui::GetIO().DisplayFramebufferScale.x;
    if (dpi <= 0.0f) {
        dpi = 1.0f;
    }
    const int cw = (std::max)(1, static_cast<int>(canvas_size.x * dpi + 0.5f));
    const int ch = (std::max)(1, static_cast<int>(canvas_size.y * dpi + 0.5f));
    // Logical pane size: the units the camera and mouse math operate in
    // (Qt-native parity, see the PaneRenderSize comment above).
    const int lw = (std::max)(1, static_cast<int>(canvas_size.x + 0.5f));
    const int lh = (std::max)(1, static_cast<int>(canvas_size.y + 0.5f));
    // Publish this frame's logical pane size for GlfwCanvas's SizeProvider
    // before anything below reads canvas.render_width()/render_height()
    // through it.
    pane_size.width = lw;
    pane_size.height = lh;
    // Sync the render target (framebuffer px, for a crisp raster) and the
    // camera projection (logical points, Qt units) to the current pane size
    // every frame -- both are cheap self-guarded no-ops when unchanged.
    // Doing it unconditionally (rather than only on a cached size change)
    // means a first-frame or buffer-switch mismatch self-corrects on the next
    // frame instead of sticking until the user resizes a pane.
    view.ensure_size(cw, ch);
    sel.resize_callback(lw, lh);
    const GLuint tex = view.render(sel);

    ImGui::Image(static_cast<ImTextureID>(tex),
                 canvas_size,
                 ImVec2(0, 1),
                 ImVec2(1, 0)); // flip V (FBO origin is bottom-left)

    // ImGui mouse coordinates are already in screen (logical) points -- the
    // same pane-logical frame the camera operates in (see PaneRenderSize),
    // so positions/deltas below are fed 1:1, exactly like the native Qt
    // canvas (its mouse scale factor render_width()/width() is 1).
    const ImVec2 img_min = ImGui::GetItemRectMin();
    const ImVec2 img_size = ImGui::GetItemRectSize();

    // Overlay an invisible button covering the image so a drag over the
    // canvas pans the image instead of moving/scrolling the enclosing
    // ImGui child: ImGui::Image is a non-interactive item, so without this
    // ImGui would not route the drag anywhere useful. While the button is
    // active (left held) it owns the mouse, so dragging keeps panning even
    // if the cursor leaves the image rect.
    ImGui::SetCursorScreenPos(img_min);
    ImGui::InvisibleButton(
        "##canvas_input", img_size, ImGuiButtonFlags_MouseButtonLeft);
    const bool canvas_hovered = ImGui::IsItemHovered();
    const bool canvas_active = ImGui::IsItemActive();

    const ImGuiIO& io = ImGui::GetIO();

    if (canvas_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        // Camera::mouse_drag_event accumulates a per-frame delta (it holds
        // no last-position state), so pass ImGui's own frame delta (logical
        // points, the camera's units) rather than an absolute position.
        const auto dx = static_cast<int>(io.MouseDelta.x);
        const auto dy = static_cast<int>(io.MouseDelta.y);
        for_each_view_target(
            ui, stages, model, sel, [&dx, &dy](const oid::Stage& s) {
                s.mouse_drag_event(dx, dy);
            });
    }

    if (canvas_hovered) {
        // Feed the live cursor position (logical points, top-down, relative
        // to the image origin) into the canvas so Camera::scroll_callback
        // can anchor zoom at the cursor instead of the corner.
        const ImVec2 mp = ImGui::GetMousePos();
        canvas.set_mouse_position(static_cast<int>(mp.x - img_min.x),
                                  static_cast<int>(mp.y - img_min.y));
        if (io.MouseWheel != 0.0f) {
            for_each_view_target(
                ui, stages, model, sel, [&io](const oid::Stage& s) {
                    s.scroll_callback(io.MouseWheel);
                });
        }
    }

    // Camera::update() (driven every frame by stage.update(), see
    // StageView::render) polls KeyboardState continuously to pan while
    // arrows are held; zoom, however, only happens on a discrete
    // key_press_event, so forward one per newly-pressed key. Not gated on
    // canvas hover (unlike drag/scroll) so keyboard zoom works like
    // keyboard pan; gated on WantCaptureKeyboard so a focused text field
    // could claim key input in the future.
    if (!io.WantCaptureKeyboard) {
        for (ImGuiKey k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END;
             k = static_cast<ImGuiKey>(k + 1)) {
            if (ImGui::IsKeyPressed(k, /*repeat=*/false)) {
                for_each_view_target(
                    ui, stages, model, sel, [&k](const oid::Stage& s) {
                        (void)s.key_press_event(static_cast<int>(k));
                    });
            }
        }
    }
}

// Qt parity: the legacy Qt frontend's imageList minimumSize is 150
// (width, 0 height; see tag legacy-qt); the left pane never shrinks
// narrower than that.
constexpr float MIN_PANE_W = 150.0f;
// Splitter handle width. The handle is flush against the list pane and
// painted, so this whole band is both visible and grabbable -- wide
// enough to make a comfortable, easy-to-see target.
constexpr float SPLITTER_W = 12.0f;

// Initializes the GLFW backend window (at the saved size/position) and the
// ImGui layer, then seeds the canvas-pane logical size to the window's
// initial size. Returns false if either backend or ImGui failed to
// initialize (main() then exits with status 1, exactly as it did inline).
bool initialize_backend_and_ui(oid::host::GlfwHostBackend& backend,
                               oid::host::ImGuiLayer& imgui,
                               const oid::host::AppSettings& loaded,
                               float& content_scale,
                               PaneRenderSize& pane_size) {
    if (!backend.initialize(
            "OpenImageDebugger (ImGui)", loaded.window_w, loaded.window_h)) {
        return false;
    }

    // Restore the saved window position only if it is still reachable (e.g.
    // not on a monitor that has since been unplugged) -- otherwise leave it
    // at whatever position the OS/window manager chose for the just-created
    // window.
    if (loaded.window_x.has_value() && loaded.window_y.has_value() &&
        oid::host::GlfwHostBackend::window_visible_on(*loaded.window_x,
                                                      *loaded.window_y,
                                                      loaded.window_w,
                                                      loaded.window_h,
                                                      backend.monitors())) {
        backend.set_window_position(*loaded.window_x, *loaded.window_y);
    }

    // HiDPI: query the window's content scale (e.g. 2x on Retina displays)
    // up front so ImGuiLayer::initialize can rasterize the UI font atlas at
    // physical resolution -- crisp text instead of a blurry upscaled bitmap
    // font. Native: thin GLFW pass-through. Non-native: the GLFW shim doesn't
    // track devicePixelRatio, so this queries it directly instead.
    content_scale = oid::platform::initial_content_scale(backend.window());

    if (!imgui.initialize(backend.window(), content_scale)) {
        return false;
    }

    // Canvas-pane logical size (see the PaneRenderSize comment above),
    // seeded to the window's initial logical size so GlfwCanvas's
    // SizeProvider below never reports 0x0 before the first draw_canvas_pane
    // call updates it to the actual pane size.
    int ww = 0;
    int wh = 0;
    glfwGetWindowSize(backend.window(), &ww, &wh);
    pane_size.width = (std::max)(1, ww);
    pane_size.height = (std::max)(1, wh);

    return true;
}

// Constructs the shared GlfwCanvas (SizeProvider bound to `pane_size`) and
// resolves its OpenGL entry points. Returns false (after printing the same
// diagnostic main() used to print inline) if entry-point resolution fails.
bool create_canvas(GLFWwindow* window,
                   PaneRenderSize& pane_size,
                   std::shared_ptr<oid::host::GlfwCanvas>& canvas) {
    // Stage's shared_ptr<RenderCanvas> needs shared ownership of the
    // GlfwCanvas; since GlfwCanvas is non-copyable (it owns a unique_ptr GL
    // entry-point table), make_shared is the clean way to get it into a
    // shared_ptr without an aliasing/no-op-deleter stack-lifetime hazard.
    // The SizeProvider makes render_width()/render_height() report the
    // canvas PANE's render size (pane_size, kept live by draw_canvas_pane)
    // rather than the whole window's framebuffer size -- see the
    // PaneRenderSize comment above for why that distinction matters.
    canvas = std::make_shared<oid::host::GlfwCanvas>(window, [&pane_size] {
        return std::make_pair(pane_size.width, pane_size.height);
    });
    if (!canvas->load()) {
        std::cerr << "[Error] failed to resolve OpenGL entry points\n";
        return false;
    }
    return true;
}

// Aggregates references to the frame loop's per-main() state so the frame
// lambda's body (originally ~180 lines, all under one capture list) can be
// split into named helpers below without re-threading each one's own
// parameter list. Every member is a reference to a main()-local that
// outlives the frame loop (the loop runs to completion inside main()), so
// holding references here is safe; nothing here is copied out of the
// locals the lambda used to mutate through its capture list.
struct FrameContext {
    oid::host::IpcClient& ipc;
    oid::host::UiState& ui;
    oid::host::ThumbnailCache& thumbnails;
    oid::host::IpcBufferModel& model;
    oid::host::GlfwHostBackend& backend;
    bool& goto_open;
    oid::host::StageManager& stages;
    oid::host::SvgIconCache& svg_icons;
    float& left_pane_w;
    oid::host::ExportDialogState& export_dialog;
    std::string& last_export_dir;
    std::shared_ptr<oid::host::GlfwCanvas>& canvas;
    oid::host::StageView& view;
    PaneRenderSize& pane_size;
    std::set<std::string, std::less<>>& seen_this_session;
    std::vector<oid::host::PreviousBuffer>& prev_buffers;
    oid::platform::SessionBridge& session_bridge;
    oid::host::SettingsSaver& saver;
    oid::host::FileOpenQueue& file_open_queue;
#if !defined(__EMSCRIPTEN__)
    // Non-null only when OID_AGENT=1; see the construction site in main()
    // and the per-frame drain() call below. Native-only: the Emscripten
    // build has no asio transport (its agent glue lives in the out-of-tree
    // platform port), so the whole endpoint is compiled out there.
    oid::host::agent::AgentServer* agent = nullptr;
#endif
};

#if !defined(__EMSCRIPTEN__)
// Dispatches agent requests queued since the last rendered frame on this
// (the GL) thread. Native-only: the Emscripten build has no agent endpoint
// (its glue lives in the out-of-tree platform port), so this is a no-op.
void drain_agent(FrameContext& ctx) {
    if (ctx.agent != nullptr) {
        ctx.agent->drain();
    }
}
#else
void drain_agent(FrameContext& /*ctx*/) {}
#endif

// Drains inbound IPC and reconciles the buffer-list thumbnail cache for
// this frame; must run before any panel below reads the model.
void poll_ipc_and_update_thumbnails(FrameContext& ctx) {
    // Drain inbound IPC before drawing anything this frame, so the
    // model (and thus every panel below) reflects the debugger's
    // latest state, and refresh the symbol-search candidate list
    // from it.
    ctx.ipc.poll();
    ctx.ui.set_available_symbols(ctx.ipc.available_symbols());

    // Buffer-list thumbnails: reset the per-frame icon-render budget
    // and drop cached textures for buffers no longer in the model,
    // now that ipc.poll() above has reconciled the model for this
    // frame.
    ctx.thumbnails.begin_frame();
    std::vector<std::string> live_buffer_names;
    live_buffer_names.reserve(ctx.model.size());
    for (std::size_t i = 0; i < ctx.model.size(); ++i) {
        live_buffer_names.push_back(ctx.model.variable_name_of(i));
    }
    ctx.thumbnails.evict_missing(live_buffer_names);
}

// Draws the menu bar and handles the frame's global keyboard shortcuts
// (quit, go-to toggle, symbol-search focus). Returns whether the symbol
// search box should claim focus this frame (draw_main_ui's search panel
// acts on it).
bool process_menu_and_shortcuts(FrameContext& ctx) {
    bool request_quit = false;
    bool request_open = false;
    oid::host::draw_menu_bar(request_quit, request_open);
    if (request_quit) {
        glfwSetWindowShouldClose(ctx.backend.window(), 1);
    }

    // Primary shortcut modifier: accept EITHER Ctrl or Cmd/Super so
    // the shortcuts follow each platform's convention without a
    // compile-time split. On macOS the Qt app binds "Ctrl+..." to Cmd,
    // so Cmd must work; on Linux/Windows it is Ctrl. This matters for
    // non-native builds too: the GLFW shim maps the host's metaKey to
    // GLFW_MOD_SUPER -> io.KeySuper, so a compile-time __APPLE__ split
    // (undefined on non-native builds) would wrongly force Ctrl-only
    // even on macOS. Accepting both is harmless: Ctrl+K/Ctrl+L collide
    // with nothing, and Ctrl stays as a reliable fallback wherever the
    // host reserves Cmd (e.g. a full tab's Cmd+L address bar; the
    // embedding host's webview does deliver Cmd).
    const bool shortcut_mod = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper;

    // Ctrl+L (Cmd+L on macOS) toggles the go-to widget (parity
    // with the Qt app's toggle_go_to_dialog() global QShortcut). A
    // modified key is never text input, so this fires even while a
    // text field holds focus -- NOT gated on WantCaptureKeyboard
    // (see shortcuts.h).
    if (oid::host::should_fire_ctrl_shortcut(
            shortcut_mod, ImGui::IsKeyPressed(ImGuiKey_L, /*repeat=*/false))) {
        ctx.goto_open = !ctx.goto_open;
    }
    oid::host::draw_goto_panel(
        ctx.ui, ctx.stages, ctx.goto_open, ctx.svg_icons);

    // Ctrl+O (Cmd+O on macOS) opens the native file dialog, same as
    // File > Open. Like Ctrl+L, it fires regardless of keyboard capture
    // and reuses the same platform modifier (shortcut_mod).
    if (oid::host::should_fire_ctrl_shortcut(
            shortcut_mod, ImGui::IsKeyPressed(ImGuiKey_O, /*repeat=*/false))) {
        request_open = true;
    }
    if (request_open) {
        ctx.file_open_queue.push_all(
            oid::platform::request_open_files(ctx.backend.window()));
    }

    // Ctrl+K (Cmd+K on macOS) focuses the symbol search box
    // (parity with the Qt app's global QShortcut calling
    // symbolList->setFocus()). Computed once per frame, before the
    // panel draws, so draw_symbol_search() can act on it the same
    // frame. Like Ctrl+L, it fires regardless of keyboard capture
    // (a modified key is not text input) and reuses the same
    // platform modifier (shortcut_mod).
    bool focus_symbol_search = false;
    if (oid::host::should_fire_ctrl_shortcut(
            shortcut_mod, ImGui::IsKeyPressed(ImGuiKey_K, /*repeat=*/false))) {
        focus_symbol_search = true;
    }
    return focus_symbol_search;
}

// Decodes and upserts every file path queued so far, whether seeded once
// from the `-o`/`--open` CLI flags or pushed by File > Open / Ctrl+O on any
// later frame. Reports the outcome on the status bar (success) or stderr
// (failures), mirroring the FileOpenQueue unit tests'
// succeeded/failed/last_error/last_success fields.
void process_pending_file_opens(FrameContext& ctx) {
    if (ctx.file_open_queue.empty()) {
        return;
    }

    const oid::host::FileOpenOutcome outcome = ctx.file_open_queue.drain(
        [](const std::string& path) {
            return oid::host::load_buffer_from_file(path);
        },
        [&ctx](oid::host::BufferRecord record) {
            ctx.model.upsert(std::move(record));
        });

    if (outcome.failed > 0) {
        std::fprintf(stderr,
                     "OpenImageDebugger: failed to open %d file(s); last "
                     "error: %s\n",
                     outcome.failed,
                     outcome.last_error.c_str());
    }

    if (outcome.succeeded > 0) {
        ctx.ui.set_status_message(std::format(
            "Opened {} ({} total)", outcome.last_success, outcome.succeeded));
    } else if (outcome.failed > 0) {
        ctx.ui.set_status_message(
            std::format("Failed to open file: {}", outcome.last_error));
    }
}

// Draws the app-chrome host body: menu-bar-adjacent layout math, the left
// (buffer list) pane, the draggable splitter, the right (canvas) pane, and
// the status bar. `focus_symbol_search` comes from
// process_menu_and_shortcuts (Ctrl+K), computed earlier in the frame so the
// symbol-search panel can act on it this same frame.
void draw_main_ui(FrameContext& ctx, bool focus_symbol_search) {
    // Host body: fills the viewport work area (BeginMainMenuBar
    // already shrank vp->WorkPos/WorkSize to sit below the menu
    // bar). Fixed pos/size, no title bar, no move/resize/scrollbar
    // -- this is the app-chrome frame the LEFT (buffer list) and
    // RIGHT (canvas) panes and the status bar sit inside.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    // Qt parity: centralwidget's QHBoxLayout has 4/4/4/4 margins
    // (leftMargin/topMargin/rightMargin/bottomMargin; see tag
    // legacy-qt).
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    if (const ImGuiWindowFlags host_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::Begin("##host_body", nullptr, host_flags)) {
        // Reserve the status-bar strip from font metrics (a
        // Separator + one text line) rather than a fixed literal, so
        // it scales with the HiDPI-rasterized UI font set up in
        // setup_ui_fonts() -- otherwise the status text clips on
        // Retina displays.
        const float status_h = ImGui::GetTextLineHeightWithSpacing() +
                               ImGui::GetStyle().ItemSpacing.y;
        const float avail_h =
            (std::max)(ImGui::GetContentRegionAvail().y - status_h, 0.0f);
        // Qt parity for the splitter's right-hand stop: QSplitter
        // stops when frame_image hits its layout minimum, which is
        // dominated by the toolbar row (9 fixed-26px buttons +
        // "Format:" label + the 100px combo, with a spacing gap
        // between each of the 11 items). Reserve that width for the
        // canvas pane instead of a flat MIN_PANE_W.
        const float min_canvas_w = 9.0f * 26.0f +
                                   ImGui::CalcTextSize("Format:").x + 100.0f +
                                   10.0f * ImGui::GetStyle().ItemSpacing.x;
        const float max_pane_w = (std::max)(ImGui::GetContentRegionAvail().x -
                                                min_canvas_w - SPLITTER_W,
                                            MIN_PANE_W);
        ctx.left_pane_w = std::clamp(ctx.left_pane_w, MIN_PANE_W, max_pane_w);

        // Left pane: buffer list (icon thumbnails, text rows,
        // selection, delete -- see thumbnail_cache.h).
        ImGui::BeginChild(
            "##list_pane", ImVec2(ctx.left_pane_w, avail_h), true);
        // Symbol search sits above the buffer list (parity with the
        // Qt app's layout: SymbolSearchInput above the buffer list
        // widget).
        oid::host::draw_symbol_search(ctx.ui, ctx.ipc, focus_symbol_search);
        oid::host::draw_buffer_list(ctx.ui,
                                    ctx.model,
                                    ctx.ipc,
                                    ctx.stages,
                                    ctx.thumbnails,
                                    ctx.export_dialog,
                                    ctx.last_export_dir);
        ImGui::EndChild();

        // Draggable splitter handle, flush against the list pane's
        // right edge. ctx.left_pane_w refers to a main() local, so
        // the width persists frame to
        // frame and
        // across selection changes (parity with QSplitter's draggable
        // handle).
        //
        // SameLine(0,0) on both sides is load-bearing: a plain
        // SameLine() inserts the default ItemSpacing.x (~8px) of dead,
        // non-grabbable space before AND after this button, which both
        // offsets the 6px grab band ~8px to the right of the visible
        // divider the user aims at and leaves an 8px gap before the
        // canvas begins. The net effect is a splitter that only grabs
        // from a narrow sliver (feels like it "works only sometimes")
        // while clicks just past it fall onto the canvas and pan the
        // image (feels like "the background drags the image"). On
        // non-native builds the resize cursor is also stubbed out by
        // the GLFW shim, so a mis-aimed grab gives no feedback -- hence
        // painting the handle (below) so there is something visible to
        // aim at.
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::InvisibleButton("##vsplit", ImVec2(SPLITTER_W, avail_h));
        const bool split_hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
        if (split_hot) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (ImGui::IsItemActive()) {
            ctx.left_pane_w =
                std::clamp(ctx.left_pane_w + ImGui::GetIO().MouseDelta.x,
                           MIN_PANE_W,
                           max_pane_w);
        }
        // Paint the handle so it reads as a grabbable divider.
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetItemRectMin(),
            ImGui::GetItemRectMax(),
            ImGui::GetColorU32(split_hot ? ImGuiCol_SeparatorActive
                                         : ImGuiCol_Separator));

        ImGui::SameLine(0.0f, 0.0f);

        // Right pane: the canvas. NoScrollWithMouse so the wheel
        // reaches zoom instead of scrolling the child.
        ImGui::BeginChild("##canvas_pane",
                          ImVec2(0, avail_h),
                          false,
                          ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse);
        // Qt parity: frame_image's QVBoxLayout (see tag legacy-qt)
        // spaces its rows (toolbar, canvas) 3px apart; only the Y
        // component changes so horizontal spacing inside the
        // toolbar row is unaffected.
        ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 3.0f);
        // Toolbar first so draw_canvas_pane's
        // GetContentRegionAvail() (used to size the offscreen
        // texture below) reflects the space left after this row,
        // not the whole pane.
        oid::host::draw_toolbar(ctx.ui, ctx.stages, ctx.model, ctx.goto_open);
        // Contrast min/max editor for the selected buffer, above
        // the canvas (parity with the legacy Qt frontend's
        // minMaxEditor row in frame_image's QVBoxLayout; see tag
        // legacy-qt); shown only while the toolbar's acEdit toggle
        // is on (parity with Qt's acEdit -> minMaxEditor visibility
        // binding).
        if (ctx.ui.ac_editor_visible()) {
            oid::host::draw_contrast_panel(ctx.ui, ctx.stages, ctx.svg_icons);
        }
        if (oid::Stage* sel = ctx.stages.selected_stage(ctx.ui.selected());
            sel != nullptr) {
            draw_canvas_pane(*ctx.canvas,
                             ctx.view,
                             *sel,
                             ctx.ui,
                             ctx.stages,
                             ctx.model,
                             ctx.pane_size);
        }
        // else: selected buffer's Stage failed to initialize (or
        // the model is empty); skip rendering the canvas this
        // frame, but keep the rest of the UI going.
        ImGui::PopStyleVar();
        ImGui::EndChild();

        ImGui::Separator();
        oid::host::draw_status_bar(ctx.ui, ctx.model, ctx.stages, *ctx.canvas);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

// Looks up the confirmed export's target buffer (by model index -> live
// Stage -> Buffer component, mirroring the Qt app's
// UIEventHandler::export_buffer() lookup chain; see panel_accessors.h's
// buffer_of()) and performs the export if found. Early-returns (leaving
// `status` empty) on each lookup miss instead of nesting three deep,
// preserving the original guard order and side effects exactly.
void export_confirmed_buffer(FrameContext& ctx, std::string& status) {
    const auto idx = ctx.ui.model_index_of(ctx.export_dialog.buffer_name);
    if (!idx.has_value()) {
        return;
    }
    oid::Stage* stage = ctx.stages.stage_for(*idx);
    if (stage == nullptr) {
        return;
    }
    const oid::Buffer* buffer = oid::host::buffer_of(*stage);
    if (buffer == nullptr) {
        return;
    }
    oid::platform::perform_export(
        *buffer, ctx.export_dialog, ctx.ipc, status, ctx.last_export_dir);
}

// Export request pump: an ImGui action queues a pending export request,
// then confirm_export shows the native OS save dialog (native) or consumes
// the request (non-native); on confirmation the selected buffer is
// exported and the outcome is recorded in the status bar. A successful
// export also updates `last_export_dir` from the saved file's parent
// directory, so the next dialog open (and the persisted settings snapshot)
// default to it.
void handle_export_requests(FrameContext& ctx) {
    if (!oid::platform::confirm_export(ctx.export_dialog)) {
        return;
    }
    // perform_export always sets a non-empty status, so an empty
    // status afterwards means the buffer lookup failed (deleted
    // between dialog-open and confirm).
    std::string status;
    export_confirmed_buffer(ctx, status);
    if (status.empty()) {
        status = "Export failed: " + ctx.export_dialog.buffer_name;
    }
    ctx.ui.set_status_message(status);
}

// Recomputes the merged previous-buffers list, builds a live settings
// snapshot, and hands it to the saver (debounced disk/IPC write).
void persist_settings_if_dirty(FrameContext& ctx) {
    // Settings persistence: recompute the merged
    // previous-buffers list from this frame's model contents, build
    // a live AppSettings snapshot, and hand it to the saver, which
    // debounces the actual disk write. Cheap: bounded by the number
    // of currently-loaded buffers, no I/O unless the saver decides
    // to flush.
    for (std::size_t i = 0; i < ctx.model.size(); ++i) {
        if (ctx.model.at(i).kind == oid::host::BufferKind::LOCAL_FILE) {
            continue;
        }
        ctx.seen_this_session.insert(ctx.model.variable_name_of(i));
    }
    std::vector<std::string> loaded_names;
    loaded_names.reserve(ctx.model.size());
    for (std::size_t i = 0; i < ctx.model.size(); ++i) {
        if (ctx.model.at(i).kind == oid::host::BufferKind::LOCAL_FILE) {
            continue;
        }
        loaded_names.push_back(ctx.model.variable_name_of(i));
    }
    const auto now_s = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    ctx.prev_buffers = oid::host::merge_previous_buffers(
        ctx.prev_buffers, loaded_names, ctx.seen_this_session, now_s);

    oid::host::AppSettings live;
    const auto [ww, wh] = ctx.backend.window_size();
    const auto [wx, wy] = ctx.backend.window_position();
    live.window_w = ww;
    live.window_h = wh;
    live.window_x = wx;
    live.window_y = wy;
    live.left_pane_w = ctx.left_pane_w;
    live.contrast_enabled = ctx.ui.contrast_enabled();
    live.link_views = ctx.ui.link_views();
    live.previous_buffers = ctx.prev_buffers;
    live.last_export_dir = ctx.last_export_dir;
    // Hold off saving until the platform says persisting is safe
    // (see SessionBridge::can_persist -- non-native builds gate on the
    // embedding host's first real session-state update, so a
    // default/empty snapshot doesn't get echoed back and overwrite the
    // persisted buffer list; native is always safe).
    if (ctx.session_bridge.can_persist()) {
        ctx.saver.update(live, glfwGetTime());
    }
}

} // namespace

int main(int argc, char** argv) {
    // Qt-free CLI parse (mirrors the Qt app's --hostname/--port flags, see
    // parse_connection_settings() in src/main.cpp -- not reused here since
    // it's Qt-coupled), plus the repeatable -o/--open file-open flags this
    // frontend adds on top. Defaults match the bridge's default listen port.
    // Unrecognized args (e.g. a stray "-style fusion" the bridge may still
    // pass on the Qt side) are ignored.
    const oid::host::CliOptions cli = oid::host::parse_cli(argc, argv);
    const oid::platform::Endpoint endpoint{
        cli.hostname, static_cast<unsigned short>(cli.port)};

    // Wires the inbound message hook (non-native only; no-op native) so the
    // embedding host can push buffer data into the module; must be installed
    // before the transport (below) starts polling for inbound messages.
    oid::platform::install_platform_hooks();

    // Load persisted window geometry / UI prefs / previous-buffer list
    // (Qt-free JSON settings) before creating the window, so the
    // window is created at the saved size directly rather than resized
    // after the fact. load() never throws: a missing/corrupt settings file
    // just yields AppSettings{} defaults, so this never blocks startup.
    oid::platform::SettingsBackend settings_backend;
    const oid::host::AppSettings loaded = settings_backend.load();

    oid::host::GlfwHostBackend backend;
    oid::host::ImGuiLayer imgui;
    float content_scale = 0.0f;
    PaneRenderSize pane_size{};
    if (!initialize_backend_and_ui(
            backend, imgui, loaded, content_scale, pane_size)) {
        return 1;
    }

    std::shared_ptr<oid::host::GlfwCanvas> canvas;
    if (!create_canvas(backend.window(), pane_size, canvas)) {
        return 1;
    }

    oid::host::IpcBufferModel model;
    // Register the model as the sink for file bytes an embedding host pushes
    // in (non-native only; no-op native). Must run before the loop so an
    // early host-driven open finds the sink installed.
    oid::platform::register_file_open_sink(model);
    // Transport platform seam: Asio TCP on native, PostMessageTransport on
    // non-native builds. Connects (bounded, non-throwing) in its ctor; if the
    // bridge isn't listening yet (or ever), the transport just marks itself
    // disconnected and ipc.poll() below becomes a no-op each frame -- the
    // app still runs with an empty buffer list rather than failing to
    // start.
    const auto transport =
        oid::platform::make_transport({endpoint.host, endpoint.port});
    oid::host::IpcClient ipc{*transport, model};
    oid::host::UiState ui{model};

    // Left pane (buffer list) width in screen points; set by apply_settings
    // below (startup: from the loaded settings; non-native: also every time a
    // session-state update arrives mid-session). Lives here (not `static`) so
    // it persists across frames as the user drags the splitter via the
    // FrameContext reference (parity with the Qt app's QSplitter, which
    // remembers its handle position for the life of the window).
    float left_pane_w = 0.0f;

    // Export dialog's default path (see `export_dialog` below) and the
    // persisted previous-buffer list; hoisted up here -- alongside
    // `left_pane_w` -- so apply_settings's explicit captures can reach them
    // too:
    // a restored/pushed settings snapshot (native startup, or a non-native
    // mid-session session-state update) must feed back into the outgoing
    // per-frame settings snapshot built near the end of the frame lambda
    // below, the same way left_pane_w already does.
    std::string last_export_dir = loaded.last_export_dir;
    std::vector<oid::host::PreviousBuffer> prev_buffers =
        loaded.previous_buffers;

    // Applies a settings snapshot's session-level fields to live UI state.
    // Used both for native startup (from the on-disk settings file, once)
    // and, on non-native builds, every time the embedding host sends a
    // session-state update mid-session (see
    // ipc.set_session_state_callback below) -- sharing this lambda means
    // the two call sites can't drift apart.
    auto apply_settings =
        [&ui, &left_pane_w, &last_export_dir, &prev_buffers, &ipc](
            const oid::host::AppSettings& s) {
            ui.set_contrast_enabled(s.contrast_enabled);
            ui.set_link_views(s.link_views);
            left_pane_w = s.left_pane_w;
            last_export_dir = s.last_export_dir;
            prev_buffers = s.previous_buffers;
            // Seed the previous-session buffer list; IpcClient auto-re-requests
            // each one (if still available and not expired) the next time the
            // bridge sends SET_AVAILABLE_SYMBOLS, so previously-plotted buffers
            // reappear once the debugger reconnects.
            ipc.set_restore_buffers(s.previous_buffers);
        };
    apply_settings(loaded);

    oid::host::StageManager stages{canvas, model};
    // Buffer-list thumbnail icon cache; declared after
    // `canvas` (which it holds a reference to) and before the frame loop, so
    // it's destroyed -- deleting its cached GL textures -- before the GLFW
    // window/context that owns those textures goes away. Reuses the same
    // `content_scale` queried above for the UI font atlas so the offscreen
    // render size (Qt parity: 100x75 base, see thumbnail_cache.h) scales
    // with the window's content scale the same way the fonts do.
    oid::host::ThumbnailCache thumbnails{*canvas, content_scale};

    // Go-to widget's x/y vector icons (Qt parity: the legacy Qt frontend's
    // go-to widget; see tag legacy-qt); like `thumbnails` above, declared
    // after `canvas` and before the frame loop
    // so its cached GL textures are deleted before the GLFW window/context
    // that owns them goes away. Unlike ThumbnailCache it holds no reference
    // to `canvas` (it dispatches GL directly, not through GlfwCanvas), but
    // still needs a live GL context for as long as it holds textures, so the
    // same declaration-order rule applies.
    oid::host::SvgIconCache svg_icons{content_scale};

    oid::host::StageView view{*canvas};

#if !defined(__EMSCRIPTEN__)
    // Native agent endpoint: off unless OID_AGENT=1. `agent_model` adapts
    // the same model/stages/ui/canvas the ImGui frontend already renders
    // through; `agent_server` owns the localhost transport and queues
    // requests for the per-frame drain() call below to dispatch on this
    // (the GL) thread. Native-only: the Emscripten build supplies its own
    // agent glue via the out-of-tree platform port and has no asio.
    std::optional<oid::host::agent::NativeViewModel> agent_model;
    std::optional<oid::host::agent::AgentServer> agent_server;
    if (const char* v = std::getenv("OID_AGENT");
        v && std::string_view(v) == "1") {
        // Socket bind/listen and discovery-file writes below can throw;
        // the agent endpoint is optional, so a failure here must not
        // abort viewer startup -- fall back to running without it.
        try {
            agent_model.emplace(model,
                                stages,
                                ui,
                                /*viewport source*/ canvas);
            agent_server.emplace(*agent_model,
                                 oid::host::agent::AgentServerConfig{
                                     /*enabled=*/true, cli.agent_debugger_pid});
        } catch (const std::exception& e) {
            agent_server.reset();
            agent_model.reset();
            std::cerr << "[oid] agent endpoint disabled: " << e.what() << "\n";
        }
    }
#endif

    // Go-to widget's open flag, kept alive across frames via FrameContext.
    bool goto_open = false;

    // Export dialog: one long-lived ExportDialogState
    // reused across every open/close cycle (buffer_list_panel's
    // right-click "Export buffer" item opens it via open_export_dialog());
    // `last_export_dir` (declared above, alongside left_pane_w) seeds its
    // default path and is updated on a successful export, feeding the
    // settings snapshot below the same way `left_pane_w` does.
    oid::host::ExportDialogState export_dialog;

    // Session/export platform bridge: on non-native builds the embedding host
    // wires a mid-session session-state update (feeds back into
    // apply_settings above) and an "export selected buffer" command (opens
    // export_dialog for the currently-selected buffer, mirroring the legacy
    // Qt frontend's exportSelectedBufferRequested wiring; see tag legacy-qt);
    // native wires nothing. Constructed after apply_settings and
    // export_dialog above (both are captured/used by the callbacks) and
    // before the frame loop below.
    oid::platform::SessionBridge session_bridge{
        ipc,
        [&apply_settings](const oid::host::AppSettings& s) {
            apply_settings(s);
        },
        [&ui, &model, &export_dialog, &last_export_dir] {
            const std::size_t idx = ui.selected();
            if (idx < model.size()) {
                oid::host::open_export_dialog(export_dialog,
                                              model.variable_name_of(idx),
                                              last_export_dir);
            }
        }};

    // Debounced/atomic settings persistence: the frame lambda
    // below builds a live AppSettings snapshot each frame and hands it to
    // the saver, which only actually writes to disk after `debounce_s`
    // elapses since the last write, so rapid changes (e.g. dragging the
    // splitter) don't hammer the filesystem. `seen_this_session` and
    // `prev_buffers` (declared above, alongside left_pane_w) track which
    // buffers have been loaded so far so the final saved list can drop ones
    // the user explicitly deleted (see merge_previous_buffers) rather than
    // treating a deletion as "not yet reloaded".
    //
    // On non-native builds there is no local settings file: the sink instead
    // pushes the snapshot to the embedding host as a session-state-changed
    // message (mirrors the legacy Qt build's settingsPersistenceRequested
    // wiring), which persists it and can push it back as a session-state
    // update (see apply_settings above). No exit flush is needed there -- the
    // frame loop never returns on non-native builds.
    oid::host::SettingsSaver saver{loaded,
                                   settings_backend.make_save_sink(ipc)};
    std::set<std::string, std::less<>> seen_this_session;

    // Seeded once from the -o/--open CLI flags; File > Open / Ctrl+O also
    // push into it later (see process_menu_and_shortcuts above), and
    // process_pending_file_opens drains it (decode + upsert) every frame.
    oid::host::FileOpenQueue file_open_queue;
    file_open_queue.push_all(cli.open_files);

    // FrameContext bundles the frame loop's state so the frame lambda's body
    // (originally one ~180-line block under a 19-entry capture list) can be
    // split into named helpers instead of re-threading each one's own
    // parameter list; see the FrameContext comment above. Every referenced
    // object is a main() local declared above, so all of them outlive the
    // frame loop below.
    FrameContext ctx{ipc,
                     ui,
                     thumbnails,
                     model,
                     backend,
                     goto_open,
                     stages,
                     svg_icons,
                     left_pane_w,
                     export_dialog,
                     last_export_dir,
                     canvas,
                     view,
                     pane_size,
                     seen_this_session,
                     prev_buffers,
                     session_bridge,
                     saver,
                     file_open_queue};
#if !defined(__EMSCRIPTEN__)
    ctx.agent = agent_server ? &*agent_server : nullptr;
#endif

    oid::host::FrameLoop loop{backend, [&ctx, &imgui] {
                                  // Runs on every tick() that renders a frame.
                                  poll_ipc_and_update_thumbnails(ctx);

                                  // Drain agent requests AFTER ipc.poll() so an
                                  // agent readback (list_buffers/get_buffer/
                                  // get_view) observes this frame's
                                  // freshly-reconciled model, not the previous
                                  // frame's -- drain_agent is itself a model
                                  // reader/mutator, so it honors the same
                                  // "poll IPC before reading the model"
                                  // contract as every panel below. It also
                                  // lets an agent set_view win over, rather
                                  // than be clobbered by, a buffer that arrived
                                  // over IPC this same frame. drain still runs
                                  // before begin_frame/render below, so an
                                  // applied set_view renders this frame. The
                                  // final tick, where the backend has requested
                                  // close, returns false before invoking this
                                  // lambda at all, so that last iteration's
                                  // drain() is skipped -- acceptable, since it
                                  // is shutdown and agent_server->stop() below
                                  // closes the socket regardless.
                                  drain_agent(ctx);

                                  // Canvas sizing + HiDPI is handled in
                                  // ImGuiLayer::begin_frame's hidpi_sync()
                                  // (non-native): it makes the canvas track its
                                  // container and drives the drawing buffer +
                                  // ImGui display metrics from the device pixel
                                  // ratio. Nothing to do here.

                                  imgui.begin_frame();

                                  const bool focus_symbol_search =
                                      process_menu_and_shortcuts(ctx);
                                  process_pending_file_opens(ctx);
                                  draw_main_ui(ctx, focus_symbol_search);
                                  handle_export_requests(ctx);

                                  imgui.render();

                                  persist_settings_if_dirty(ctx);
                              }};

    loop.run();
#if !defined(__EMSCRIPTEN__)
    // Stop accepting/serving agent connections before the GL context and
    // Stage/Buffer state below are torn down, so no in-flight drain() call
    // can touch them.
    if (agent_server) {
        agent_server->stop();
    }
#endif
    // Force a final save if a debounced write was still pending when the
    // window closed, so the last frame's geometry/prefs/buffer list aren't
    // silently dropped.
    saver.flush();
    return 0;
}
