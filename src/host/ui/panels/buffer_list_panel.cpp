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

#include "host/ui/panels/buffer_list_panel.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>

#include <imgui.h>

#include "host/ipc/ipc_client.h"
#include "host/ui/buffer_model.h"
#include "host/ui/export_dialog.h"
#include "host/ui/ipc_buffer_model.h"
#include "host/ui/stage_manager.h"
#include "host/ui/symbol_filter.h"
#include "host/ui/thumbnail_cache.h"
#include "host/ui/ui_state.h"

namespace oid::host {

namespace {

std::string row_label(const BufferRecord& rec) {
    return std::format("{}\n[{}x{}]\n{}",
                       rec.display_name,
                       rec.width,
                       rec.height,
                       type_label(rec.type, rec.channels));
}

// Up/Down move the selection through the buffer list while it is
// focused. repeat=true so holding the key keeps stepping, like the
// symbol-search completer popup. Handled before the row loop below so a
// newly-selected row is scrolled into view this same frame.
bool handle_buffer_list_navigation(UiState& ui,
                                   const IpcBufferModel& model,
                                   bool list_focused) {
    bool nav_moved = false;
    if (list_focused && ui.has_selection()) {
        const bool up = ImGui::IsKeyPressed(ImGuiKey_UpArrow, /*repeat=*/true);
        const bool down =
            ImGui::IsKeyPressed(ImGuiKey_DownArrow, /*repeat=*/true);
        if (up || down) {
            // Same clamp-step helper the autocomplete uses: steps by one and
            // clamps to [0, size-1] (no wrap-around).
            const int new_sel =
                symbol_completion_nav(static_cast<int>(ui.selected()),
                                      static_cast<int>(model.size()),
                                      up,
                                      down);
            if (static_cast<std::size_t>(new_sel) != ui.selected()) {
                ui.select(static_cast<std::size_t>(new_sel));
                nav_moved = true;
            }
        }
    }
    return nav_moved;
}

// Right-click context menu: "Export buffer" opens the export dialog
// for this row's buffer (parity with the legacy Qt frontend's
// imageList context menu and its show_context_menu() handler; see
// tag legacy-qt). Triggers anywhere in the row, including the
// thumbnail.
void draw_buffer_row_context_menu(ExportDialogState& export_dialog,
                                  const std::string& name,
                                  const std::string& last_export_dir) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Export buffer")) {
            open_export_dialog(export_dialog, name, last_export_dir);
        }
        ImGui::EndPopup();
    }
}

// Stable UI context shared by every row drawn this frame, grouped so
// draw_buffer_list_row() stays under Sonar's parameter-count limit. Holds
// references only; the per-row `i` and `nav_moved` stay plain parameters
// since they change on every call.
struct BufferListRowContext {
    UiState& ui;
    const IpcBufferModel& model;
    StageManager& stages;
    ThumbnailCache& thumbs;
    ExportDialogState& export_dialog;
    const std::string& last_export_dir;
    const ImGuiStyle& style;
};

void draw_buffer_list_row(const BufferListRowContext& ctx,
                          std::size_t i,
                          bool nav_moved) {
    ImGui::PushID(static_cast<int>(i));

    const ImVec2 row_start = ImGui::GetCursorScreenPos();

    const std::string& name = ctx.model.variable_name_of(i);
    const GLuint tex = ctx.thumbs.texture_for(
        name, ctx.model.revision_of(i), ctx.stages.stage_for(i));

    // The 3-line label can be taller than the icon (HiDPI font scaling),
    // so the row takes whichever is taller; both overlays center in it.
    const std::string label = row_label(ctx.model.at(i));
    const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
    const float row_h =
        (std::max)(static_cast<float>(ThumbnailCache::DISPLAY_H), text_size.y);

    // Full-width invisible Selectable submitted first: owns row's target
    // hover/selection highlight (Qt QListWidget parity — thumbnail, label,
    // empty space to pane edge all select). Thumbnail + label overlaid
    // afterwards as non-interactive items, Selectable underneath keeps
    // hover and click.
    if (ImGui::Selectable("##row",
                          ctx.ui.selected() == i,
                          ImGuiSelectableFlags_AllowDoubleClick,
                          ImVec2(0, row_h))) {
        ctx.ui.select(i);
    }

    // Keep the row Up/Down just landed on visible; only when keyboard
    // navigation moved the selection this frame, so mouse scrolling isn't
    // yanked back to the selected row.
    if (nav_moved && ctx.ui.selected() == i) {
        ImGui::SetScrollHereY(0.5f);
    }

    draw_buffer_row_context_menu(ctx.export_dialog, name, ctx.last_export_dir);

    // Where the Selectable's layout put the next row — restored after
    // the overlays so consecutive rows stack without extra gaps.
    const ImVec2 after_row = ImGui::GetCursorScreenPos();

    if (tex != 0) {
        ImGui::SetCursorScreenPos(
            ImVec2(row_start.x,
                   row_start.y +
                       (row_h - static_cast<float>(ThumbnailCache::DISPLAY_H)) *
                           0.5f));
        ImGui::Image(
            static_cast<ImTextureID>(static_cast<intptr_t>(tex)),
            ImVec2(ThumbnailCache::DISPLAY_W, ThumbnailCache::DISPLAY_H));
    }

    ImGui::SetCursorScreenPos(ImVec2(
        row_start.x + ThumbnailCache::DISPLAY_W + ctx.style.ItemSpacing.x,
        row_start.y + (row_h - text_size.y) * 0.5f));
    ImGui::TextUnformatted(label.c_str());

    ImGui::SetCursorScreenPos(after_row);

    ImGui::PopID();
}

// Delete removes the selected buffer. Focus alone is the exact gate: the
// child is focused only once the user clicked a row, and unfocused the
// moment focus moves to the search box or canvas, so a Delete meant for
// that text field can't remove a buffer. Don't gate on
// !io.WantCaptureKeyboard: the list itself claims the keyboard via
// SetNextFrameWantCaptureKeyboard in draw_buffer_list, which would make
// that flag true and block Delete. Delete OR Backspace: on macOS the main
// "delete" key reports as Backspace (forward-delete is Fn+Delete), so
// accept both.
void handle_buffer_list_delete_key(const UiState& ui,
                                   IpcBufferModel& model,
                                   IpcClient& ipc,
                                   bool list_focused) {
    if (list_focused && ui.has_selection() &&
        (ImGui::IsKeyPressed(ImGuiKey_Delete, /*repeat=*/false) ||
         ImGui::IsKeyPressed(ImGuiKey_Backspace, /*repeat=*/false))) {
        // Notify the bridge before mutating the local model, then remove by
        // name (IpcBufferModel is keyed by variable_name, not a stable
        // index). StageManager is not touched here: its sync() drops the
        // removed buffer's Stage the next time a Stage is requested (see
        // stage_manager.h), so no dangling-span bookkeeping is needed here.
        const std::size_t sel = ui.selected();
        const std::string name = model.variable_name_of(sel);
        ipc.notify_removed(name);
        model.remove(name);
    }
}

} // namespace

void draw_buffer_list(UiState& ui,
                      IpcBufferModel& model,
                      IpcClient& ipc,
                      StageManager& stages,
                      ThumbnailCache& thumbs,
                      ExportDialogState& export_dialog,
                      const std::string& last_export_dir) {
    const ImGuiStyle& style = ImGui::GetStyle();

    // The buffer list lives in its own child window so it can hold keyboard
    // focus independently of the symbol-search box above it. IsWindowFocused()
    // on this child then becomes the exact "focus is on the list" signal --
    // it becomes true when the user clicks a row and clears the moment click
    // moves to the search box or canvas. The Delete gate and arrow navigation
    // below rely on it. Size (0, 0) fills the remaining pane height below the
    // search box; borderless keeps the layout visually unchanged.
    ImGui::BeginChild("##buffer_list", ImVec2(0, 0), false);

    const bool list_focused = ImGui::IsWindowFocused();

    // While the list holds focus, claim the keyboard so the shared canvas
    // camera stands down. The camera pans on BARE arrow keys
    // (Camera::handle_key_events, camera.cpp) and is suppressed only while
    // io.WantCaptureKeyboard is set (events.cpp). ImGui keyboard-nav is
    // off, so a focused plain child never sets that flag on its own -- without
    // this call, Up/Down would pan the canvas AND move the selection at once.
    // This mirrors how the symbol-search text field implicitly captures the
    // keyboard; it takes effect next frame, which is seamless because focus
    // persists across frames.
    if (list_focused) {
        ImGui::SetNextFrameWantCaptureKeyboard(true);
    }

    const bool nav_moved =
        handle_buffer_list_navigation(ui, model, list_focused);

    // Qt parity: the legacy Qt frontend's imageList spacing is 1 (vertical
    // gap between rows; see tag legacy-qt); only the Y component changes so
    // the thumbnail/label
    // horizontal spacing used above (style.ItemSpacing.x) is unaffected.
    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 1.0f);
    const BufferListRowContext row_ctx{.ui = ui,
                                       .model = model,
                                       .stages = stages,
                                       .thumbs = thumbs,
                                       .export_dialog = export_dialog,
                                       .last_export_dir = last_export_dir,
                                       .style = style};
    for (std::size_t i = 0; i < model.size(); ++i) {
        draw_buffer_list_row(row_ctx, i, nav_moved);
    }
    ImGui::PopStyleVar();

    handle_buffer_list_delete_key(ui, model, ipc, list_focused);

    ImGui::EndChild();
}

} // namespace oid::host
