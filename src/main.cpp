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
#include "host/frame_pacer.h"
#include "platform/app_nap.h"
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

    // The backend's sampler is linear; the stage texture is drawn 1:1 in
    // framebuffer pixels and must stay nearest, or fractional DPI blurs it.
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCallback(
        ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);
    ImGui::Image(tex,
                 canvas_size,
                 ImVec2(0, 1),
                 ImVec2(1, 0)); // flip V (FBO origin is bottom-left)
    draw_list->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear,
                           nullptr);

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

// Qt parity: the legacy Qt frontend's imageList minimum width is 150
// (see tag legacy-qt).
constexpr float MIN_PANE_W = 150.0f;
// The handle is painted and flush against the list pane, so this whole
// band is grabbable: wide enough to be an easy target.
constexpr float SPLITTER_W = 12.0f;

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
        oid::host::GlfwHostBackend::window_visible_on(
            *loaded.window_x,
            *loaded.window_y,
            loaded.window_w,
            loaded.window_h,
            oid::host::GlfwHostBackend::monitors())) {
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

// The FrameContext members read only by persist_settings_if_dirty; ones
// the layout and export paths also read stay directly on FrameContext.
struct SettingsPersistence {
    std::set<std::string, std::less<>>& seen_this_session;
    std::vector<oid::host::PreviousBuffer>& prev_buffers;
    oid::host::SettingsSaver& saver;
    oid::host::SettingsScope settings_scope;
};

// Members that are references point at main() locals that outlive the
// frame loop, so each helper takes one ctx rather than a capture list.
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
    oid::platform::SessionBridge& session_bridge;
    SettingsPersistence settings_persistence;
    oid::host::FileOpenQueue& file_open_queue;
#if !defined(__EMSCRIPTEN__)
    // Non-null only when OID_AGENT=1. Native-only: the Emscripten build has
    // no asio transport, so the whole endpoint is compiled out there.
    oid::host::agent::AgentServer* agent = nullptr;
#endif
};

#if !defined(__EMSCRIPTEN__)
// Dispatches agent requests queued since the last frame, on the GL
// thread. Native-only: the Emscripten build has no agent endpoint.
void drain_agent(const FrameContext& ctx) {
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
    ctx.ipc.poll();
    ctx.ui.set_available_symbols(ctx.ipc.available_symbols());

    ctx.thumbnails.begin_frame();
    std::vector<std::string> live_buffer_names;
    live_buffer_names.reserve(ctx.model.size());
    for (std::size_t i = 0; i < ctx.model.size(); ++i) {
        live_buffer_names.push_back(ctx.model.variable_name_of(i));
    }
    ctx.thumbnails.evict_missing(live_buffer_names);
}

// Returns whether the symbol search box should claim focus this frame.
bool process_menu_and_shortcuts(const FrameContext& ctx) {
    bool request_quit = false;
    bool request_open = false;
    oid::host::draw_menu_bar(request_quit, request_open);
    if (request_quit) {
        glfwSetWindowShouldClose(ctx.backend.window(), 1);
    }

    // Accept EITHER Ctrl or Cmd: macOS Qt binds "Ctrl+..." to Cmd, and Ctrl
    // stays the fallback where the host reserves Cmd (a tab's Cmd+L address
    // bar). A compile-time __APPLE__ split is undefined on non-native builds,
    // whose GLFW shim maps metaKey to GLFW_MOD_SUPER.
    const bool shortcut_mod = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper;

    // Ctrl/Cmd+L toggles the go-to widget (Qt parity). A modified key is
    // never text input, so this is NOT gated on WantCaptureKeyboard.
    if (oid::host::should_fire_ctrl_shortcut(
            shortcut_mod, ImGui::IsKeyPressed(ImGuiKey_L, /*repeat=*/false))) {
        ctx.goto_open = !ctx.goto_open;
    }
    oid::host::draw_goto_panel(
        ctx.ui, ctx.stages, ctx.goto_open, ctx.svg_icons);

    if (oid::host::should_fire_ctrl_shortcut(
            shortcut_mod, ImGui::IsKeyPressed(ImGuiKey_O, /*repeat=*/false))) {
        request_open = true;
    }
    if (request_open) {
        ctx.file_open_queue.push_all(
            oid::platform::request_open_files(ctx.backend.window()));
    }

    // Ctrl/Cmd+K focuses the symbol search box (Qt parity). Computed before
    // the panel draws so draw_symbol_search can act on it the same frame.
    bool focus_symbol_search = false;
    if (oid::host::should_fire_ctrl_shortcut(
            shortcut_mod, ImGui::IsKeyPressed(ImGuiKey_K, /*repeat=*/false))) {
        focus_symbol_search = true;
    }
    return focus_symbol_search;
}

void process_pending_file_opens(FrameContext& ctx) {
    if (ctx.file_open_queue.empty()) {
        return;
    }

    const auto [succeeded, failed, last_error, last_success] =
        ctx.file_open_queue.drain(
            [](const std::string& path) {
                return oid::host::load_buffer_from_file(path);
            },
            [&ctx](oid::host::BufferRecord record) {
                ctx.model.upsert(std::move(record));
            });

    if (failed > 0) {
        std::fprintf(stderr,
                     "OpenImageDebugger: failed to open %d file(s); last "
                     "error: %s\n",
                     failed,
                     last_error.c_str());
    }

    if (succeeded > 0) {
        ctx.ui.set_status_message(
            std::format("Opened {} ({} total)", last_success, succeeded));
    } else if (failed > 0) {
        ctx.ui.set_status_message(
            std::format("Failed to open file: {}", last_error));
    }
}

void draw_main_ui(const FrameContext& ctx, const bool focus_symbol_search) {
    // BeginMainMenuBar already shrank vp->WorkPos/WorkSize to sit below
    // the menu bar, so filling the work area needs no menu-height fudge.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    // Qt parity: centralwidget's QHBoxLayout has 4/4/4/4 margins (see
    // tag legacy-qt).
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    if (constexpr ImGuiWindowFlags host_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::Begin("##host_body", nullptr, host_flags)) {
        // From font metrics, not a literal, so the strip scales with the
        // HiDPI-rasterized UI font -- otherwise status text clips on Retina.
        const float status_h = ImGui::GetTextLineHeightWithSpacing() +
                               ImGui::GetStyle().ItemSpacing.y;
        const float avail_h =
            (std::max)(ImGui::GetContentRegionAvail().y - status_h, 0.0f);
        // Qt parity for the splitter's right-hand stop: frame_image's
        // layout minimum is the toolbar row (9 26px buttons, label, combo).
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
        oid::host::draw_symbol_search(ctx.ui, ctx.ipc, focus_symbol_search);
        oid::host::draw_buffer_list(ctx.ui,
                                    ctx.model,
                                    ctx.ipc,
                                    ctx.stages,
                                    ctx.thumbnails,
                                    ctx.export_dialog,
                                    ctx.last_export_dir);
        ImGui::EndChild();

        // SameLine(0,0) on both sides is load-bearing: the default
        // ItemSpacing offsets the grab band ~8px from the painted divider.
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
        // The GLFW shim stubs the resize cursor out on non-native builds,
        // so this paint is the only thing left to aim a grab at.
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
        // Qt parity: frame_image's QVBoxLayout spaces its rows 3px apart
        // (see tag legacy-qt); Y only, so toolbar spacing is unaffected.
        ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 3.0f);
        // Toolbar first so draw_canvas_pane's GetContentRegionAvail() sees
        // the space left after this row, not the whole pane.
        oid::host::draw_toolbar(ctx.ui, ctx.stages, ctx.model, ctx.goto_open);
        // Qt parity: the minMaxEditor row above the canvas in frame_image's
        // QVBoxLayout (see tag legacy-qt).
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
        // No else: a Stage that failed to initialize (or an empty model)
        // just skips the canvas this frame; the rest of the UI keeps going.
        ImGui::PopStyleVar();
        ImGui::EndChild();

        ImGui::Separator();
        oid::host::draw_status_bar(ctx.ui, ctx.model, ctx.stages, *ctx.canvas);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

// Mirrors the Qt app's UIEventHandler::export_buffer() lookup chain;
// leaves `status` empty on a miss, which handle_export_requests reports.
void export_confirmed_buffer(const FrameContext& ctx, std::string& status) {
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

// Export pump: confirm_export shows the native OS save dialog (native)
// or consumes the queued request (non-native).
void handle_export_requests(const FrameContext& ctx) {
    if (!oid::platform::confirm_export(ctx.export_dialog)) {
        return;
    }
    // perform_export always sets a non-empty status, so an empty one
    // means the buffer was deleted between dialog-open and confirm.
    std::string status;
    export_confirmed_buffer(ctx, status);
    if (status.empty()) {
        status = "Export failed: " + ctx.export_dialog.buffer_name;
    }
    ctx.ui.set_status_message(status);
}

// Runs every rendered frame: bounded by the currently-loaded buffer count,
// and the debounced saver does no I/O unless it decides to flush.
void persist_settings_if_dirty(const FrameContext& ctx) {
    for (std::size_t i = 0; i < ctx.model.size(); ++i) {
        if (ctx.model.at(i).kind == oid::host::BufferKind::LOCAL_FILE) {
            continue;
        }
        ctx.settings_persistence.seen_this_session.insert(
            ctx.model.variable_name_of(i));
    }
    std::vector<std::string> loaded_names;
    loaded_names.reserve(ctx.model.size());
    for (std::size_t i = 0; i < ctx.model.size(); ++i) {
        if (ctx.model.at(i).kind == oid::host::BufferKind::LOCAL_FILE) {
            continue;
        }
        loaded_names.push_back(ctx.model.variable_name_of(i));
    }
    const auto now_s = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    ctx.settings_persistence.prev_buffers = oid::host::merge_previous_buffers(
        ctx.settings_persistence.prev_buffers,
        loaded_names,
        ctx.settings_persistence.seen_this_session,
        now_s);

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
    live.previous_buffers = ctx.settings_persistence.prev_buffers;
    live.last_export_dir = ctx.last_export_dir;
    // Non-native gates on the embedding host's first real session-state
    // update, so a default snapshot can't overwrite the persisted list.
    if (ctx.session_bridge.can_persist()) {
        // Scoped before the saver: on a build that does not own the window
        // the geometry never settles, so an unscoped snapshot emits for ever.
        ctx.settings_persistence.saver.update(
            oid::host::settings_for_scope(
                std::move(live), ctx.settings_persistence.settings_scope),
            glfwGetTime());
    }
}

} // namespace

int main(int argc, char** argv) {
    // Qt-free CLI parse: the Qt app's --hostname/--port, plus the repeatable
    // -o/--open flags. Unrecognized args (e.g. "-style fusion") are ignored.
    const auto [hostname, port, open_files, agent_debugger_pid] =
        oid::host::parse_cli(argc, argv);
    const oid::platform::Endpoint endpoint{hostname,
                                           static_cast<unsigned short>(port)};

    // Inbound message hook (non-native only); must be installed before the
    // transport below starts polling for inbound messages.
    oid::platform::install_platform_hooks();

    // Loaded before the window is created, so it opens at the saved size
    // rather than being resized after. A corrupt file just yields defaults.
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
    // Sink for file bytes an embedding host pushes in (non-native only);
    // must run before the loop so an early host-driven open finds it.
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
        [&ui,
         &left_pane_w,
         &last_export_dir,
         &prev_buffers,
         &ipc,
         scope = settings_backend.scope()](const oid::host::AppSettings& s) {
            ui.set_contrast_enabled(s.contrast_enabled);
            ui.set_link_views(s.link_views);
            left_pane_w = s.left_pane_w;
            last_export_dir = s.last_export_dir;
            // Not redundant with the parser's scope guard: it returns
            // *defaults* for host-owned keys, which would empty prev_buffers.
            if (scope == oid::host::SettingsScope::FULL) {
                prev_buffers = s.previous_buffers;
                // IpcClient auto-re-requests each one on the next
                // SET_AVAILABLE_SYMBOLS, so plotted buffers reappear.
                ipc.set_restore_buffers(s.previous_buffers);
            }
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

    // Platform seam for a non-native port's own agent glue; native's is
    // a no-op, since its endpoint is assembled below.
    oid::platform::register_agent_targets(model, stages, ui, canvas);

#if !defined(__EMSCRIPTEN__)
    // Native agent endpoint, off unless OID_AGENT=1. `pacer` is declared
    // BEFORE agent_server, so ~AgentServer joins serve threads first.
    std::optional<oid::host::FramePacer> pacer;
    // Declared BEFORE agent_server, which holds a ViewModel& to it and is
    // used past this block via FrameContext.
    std::optional<oid::host::agent::NativeViewModel> agent_model;
    std::optional<oid::host::agent::AgentServer> agent_server;
    if (const char* v = std::getenv("OID_AGENT");
        v && std::string_view(v) == "1") {
        // Bind/listen and discovery-file writes can throw; the endpoint is
        // optional, so a failure falls back to running without it.
        try {
            agent_model.emplace(model,
                                stages,
                                ui,
                                /*viewport source*/ canvas);
            agent_server.emplace(*agent_model,
                                 oid::host::agent::AgentServerConfig{
                                     /*enabled=*/true, agent_debugger_pid});
            pacer.emplace(
                std::chrono::nanoseconds{std::chrono::seconds{1}} /
                oid::host::GlfwHostBackend::primary_refresh_rate_hz());
            agent_server->set_enqueue_listener([&p = *pacer] { p.wake(); });
            oid::platform::begin_agent_activity();
            // Last step of agent startup: an earlier throw falls back to the
            // vsync loop, so vsync is never left off on a plain loop.run().
            oid::host::GlfwHostBackend::set_vsync(false);
        } catch (const std::exception& e) {
            agent_server.reset();
            agent_model.reset();
            pacer.reset();
            // Idempotent (and a no-op if begin never ran): the fallback
            // non-agent run must not keep the App Nap opt-out active.
            oid::platform::end_agent_activity();
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
            // Refused in words: the host gets no reply, so a command arriving
            // at an empty viewer would otherwise look like a broken menu.
            if (const std::string_view refusal =
                    oid::host::export_selected_refusal(model.size());
                !refusal.empty()) {
                ui.set_status_message(std::string{refusal});
                return;
            }
            oid::host::open_export_dialog(export_dialog,
                                          model.variable_name_of(ui.selected()),
                                          last_export_dir);
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
    // Seeded scoped, not with `loaded` as-is, so "the saver never holds
    // host-owned state under VIEWER_OWNED" holds by construction.
    oid::host::SettingsSaver saver{
        oid::host::settings_for_scope(loaded, settings_backend.scope()),
        settings_backend.make_save_sink(ipc)};
    std::set<std::string, std::less<>> seen_this_session;

    oid::host::FileOpenQueue file_open_queue;
    file_open_queue.push_all(open_files);

    FrameContext ctx{
        ipc,
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
        session_bridge,
        SettingsPersistence{
            seen_this_session, prev_buffers, saver, settings_backend.scope()},
        file_open_queue};
#if !defined(__EMSCRIPTEN__)
    ctx.agent = agent_server ? &*agent_server : nullptr;
#endif

    oid::host::FrameLoop loop{backend, [&ctx, &imgui] {
                                  // Runs on every tick() that renders a frame.
                                  poll_ipc_and_update_thumbnails(ctx);

                                  // After ipc.poll() so an agent
                                  // readback sees this frame's model and
                                  // a set_view is not clobbered by a
                                  // same-frame IPC buffer; before
                                  // begin_frame so an applied set_view
                                  // renders this frame.
                                  drain_agent(ctx);

                                  // Canvas sizing + HiDPI happen in
                                  // ImGuiLayer::begin_frame's
                                  // hidpi_sync() (non-native); nothing
                                  // to do here.

                                  imgui.begin_frame();

                                  const bool focus_symbol_search =
                                      process_menu_and_shortcuts(ctx);
                                  process_pending_file_opens(ctx);
                                  draw_main_ui(ctx, focus_symbol_search);
                                  handle_export_requests(ctx);

                                  oid::host::ImGuiLayer::render();

                                  persist_settings_if_dirty(ctx);
                              }};

#if !defined(__EMSCRIPTEN__)
    if (pacer) {
        // Vsync is off, so pace() owns the cadence: a request wakes the
        // wait and drains within ~1 ms even when the window is occluded.
        while (loop.tick()) {
            pacer->pace([&ctx] {
                // Poll before draining: a gap-drained readback must not
                // answer with the previous frame's model. Bare poll(): the
                // thumbnail work in poll_ipc_and_update_thumbnails is
                // render-side, budgeted once per frame, never agent-read.
                ctx.ipc.poll();
                drain_agent(ctx);
            });
        }
    } else {
        loop.run();
    }
#else
    loop.run();
#endif

#if !defined(__EMSCRIPTEN__)
    // Stop serving before the GL context and Stage/Buffer state are torn
    // down, so no in-flight drain() can touch them.
    if (agent_server) {
        agent_server->stop();
        oid::platform::end_agent_activity();
    }
#endif
    // Force a final save if a debounced write was still pending when the
    // window closed, so the last frame's geometry/prefs/buffer list aren't
    // silently dropped.
    saver.flush();
    return 0;
}
