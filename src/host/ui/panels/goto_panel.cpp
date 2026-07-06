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

#include "host/ui/panels/goto_panel.h"

#include <cmath>
#include <cstdint>
#include <string>

#include <imgui.h>

#include "host/ui/panels/panel_accessors.h"
#include "host/ui/stage_manager.h"
#include "host/ui/svg_icon_cache.h"
#include "host/ui/text_input.h"
#include "host/ui/ui_state.h"
#include "visualization/components/camera.h"
#include "visualization/stage.h"

namespace oid::host {

namespace {

// Prefill only on the open transition, not every frame -- doing it every
// frame would stomp in-progress edits with the camera's (unchanging)
// center on every subsequent frame the widget stays open.
void prefill_goto_fields_on_open(bool& was_open,
                                 oid::Stage* sel,
                                 int& x,
                                 int& y) {
    if (!was_open) {
        was_open = true;
        if (sel != nullptr) {
            if (const oid::Camera* cam = camera_of(*sel); cam != nullptr) {
                const oid::vec4 p = cam->get_position();
                x = static_cast<int>(std::lround(p.x() - 0.5f));
                y = static_cast<int>(std::lround(p.y() - 0.5f));
            }
        }
    }
}

// Field rows: Qt-parity vector icon (the legacy Qt frontend's
// go-to widget; see tag legacy-qt) ahead of each field, sized to
// the current font so it lines up with the
// text; texture_for() returning 0 (rasterization failed, or no
// current GL context) falls back to today's plain text label.
void draw_goto_axis_field(SvgIconCache& icons,
                          int icon_px,
                          IconId icon_id,
                          const char* tooltip,
                          const char* input_id,
                          const char* fallback_label,
                          int& value) {
    if (const GLuint tex = icons.texture_for(icon_id, icon_px, icon_px);
        tex != 0) {
        ImGui::Image(
            static_cast<ImTextureID>(static_cast<intptr_t>(tex)),
            ImVec2(static_cast<float>(icon_px), static_cast<float>(icon_px)));
        ImGui::SetItemTooltip("%s", tooltip);
        ImGui::SameLine();
        input_int_field(input_id, value);
    } else {
        input_int_field(fallback_label, value);
    }
}

// Handles the Go button / Enter-to-submit path: validates the fields via
// ui.parse_goto() and, on success, calls sel->go_to_pixel() (the inverse of
// the prefill math); closes the widget either way once submitted.
void commit_goto_on_submit(const UiState& ui,
                           const oid::Stage* sel,
                           int x,
                           int y,
                           bool enter_pressed,
                           bool& window_open) {
    if (ImGui::Button("Go") || enter_pressed) {
        if (sel != nullptr &&
            ui.parse_goto(std::to_string(x), std::to_string(y)).has_value()) {
            sel->go_to_pixel(static_cast<float>(x) + 0.5f,
                             static_cast<float>(y) + 0.5f);
        }
        window_open = false;
    }
}

void close_goto_on_escape(bool focused, bool& window_open) {
    if (focused && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        window_open = false;
    }
}

} // namespace

void draw_goto_panel(const UiState& ui,
                     StageManager& stages,
                     bool& open,
                     SvgIconCache& icons) {
    // Panel-local staging for the two int fields, and whether the widget was
    // open last frame: neither belongs in UiState (that's app logic, this is
    // pure widget state), and both must persist across frames to (a) let the
    // user keep typing without being clobbered and (b) detect the
    // false->true "just opened" transition below.
    static int x = 0;
    static int y = 0;
    static bool was_open = false;

    if (!open) {
        was_open = false;
        return;
    }

    oid::Stage* sel = stages.selected_stage(ui.selected());

    prefill_goto_fields_on_open(was_open, sel, x, y);

    bool window_open = open;
    if (ImGui::Begin("Go to pixel",
                     &window_open,
                     ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoCollapse)) {
        if (sel == nullptr) {
            ImGui::TextUnformatted("No buffer selected.");
        }

        const auto icon_px = static_cast<int>(ImGui::GetFontSize());
        draw_goto_axis_field(icons,
                             icon_px,
                             IconId::GoToX,
                             "Horizontal coordinate",
                             "##x",
                             "X",
                             x);
        draw_goto_axis_field(icons,
                             icon_px,
                             IconId::GoToY,
                             "Vertical coordinate",
                             "##y",
                             "Y",
                             y);

        const bool focused =
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        const bool enter_pressed =
            focused && (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
                        ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));

        commit_goto_on_submit(ui, sel, x, y, enter_pressed, window_open);
        close_goto_on_escape(focused, window_open);
    }
    ImGui::End();

    open = window_open;
}

} // namespace oid::host
