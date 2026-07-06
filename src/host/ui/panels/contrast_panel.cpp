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

#include "host/ui/panels/contrast_panel.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

#include <imgui.h>

#include "host/ui/panels/icon_button.h"
#include "host/ui/panels/panel_accessors.h"
#include "host/ui/stage_manager.h"
#include "host/ui/svg_icon_cache.h"
#include "host/ui/ui_state.h"
#include "host/ui_fonts.h"
#include "visualization/components/buffer.h"
#include "visualization/stage.h"

namespace oid::host {

void draw_contrast_panel(const UiState& ui,
                         StageManager& stages,
                         SvgIconCache& icons) {
    oid::Stage* stage = stages.selected_stage(ui.selected());
    if (stage == nullptr) {
        return;
    }
    oid::Buffer* buffer = buffer_of(*stage);
    if (buffer == nullptr) {
        return;
    }

    // In single-channel display mode only channel 0 drives the render, so
    // only its min/max fields stay editable (parity with the Qt app's
    // update_channel_labels, which greys out the other channel widgets).
    const bool single_channel = buffer->get_display_channel_mode() == 1;
    const int channel_count = (std::min)(buffer->channels, 4);
    const std::span<float> mins = buffer->min_buffer_values();
    const std::span<float> maxs = buffer->max_buffer_values();

    // Qt parity: acToggle disables the whole editor (minMaxEditor.setEnabled)
    ImGui::BeginDisabled(!ui.contrast_enabled());
    ImGui::Indent(10.0f); // Qt gridLayout leftMargin=10

    const float field_h = ImGui::GetFrameHeight();
    const float block_h = 2.0f * field_h + ImGui::GetStyle().ItemSpacing.y;

    // lower_upper_bound icon spans both rows, vertically centered against
    // the two-row block like a Qt grid rowspan cell (label_minmax, 8x35).
    if (const GLuint lub = icons.texture_for(IconId::LowerUpperBound, 8, 35);
        lub != 0) {
        const float top_y = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(top_y +
                             (std::max)(0.0f, (block_h - 35.0f) * 0.5f));
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(lub)),
                     ImVec2(8.0f, 35.0f));
        ImGui::SetItemTooltip("Min and max intensity editor");
        ImGui::SameLine();
        ImGui::SetCursorPosY(top_y);
    }

    // Fixed column geometry, Qt-grid style: mixing SameLine() with cursor-Y
    // nudges (for the dot centering) corrupts ImGui's shared-line origin --
    // the first cell anchors to the row top but later cells inherit the
    // dot-offset line Y and drift down. Absolute positioning per cell
    // sidesteps that bookkeeping entirely.
    const float dot_w = 10.0f;
    const float dot_gap = 3.0f;  // Qt per-cell HBox spacing is 1 + label pad
    const float field_w = 50.0f; // Qt QLineEdit max width 50
    const float cell_pitch =
        dot_w + dot_gap + field_w + ImGui::GetStyle().ItemSpacing.x;

    ImGui::BeginGroup(); // the two field rows, beside the spanning icon
    for (int row = 0; row < 2; ++row) { // 0 = min (Qt row 0), 1 = max
        const ImVec2 row_pos = ImGui::GetCursorScreenPos();
        // Qt lays out all four channel columns and greys the ones past the
        // buffer's channel count (update_channel_labels) -- drawing all
        // four keeps the reset-button column at a fixed position too.
        for (int c = 0; c < 4; ++c) {
            const float cell_x = row_pos.x + static_cast<float>(c) * cell_pitch;
            ImGui::PushID(row * 4 + c);
            const bool channel_active =
                c < channel_count && !(single_channel && c != 0);
            ImGui::BeginDisabled(!channel_active);
            if (const GLuint ch = icons.texture_for(
                    static_cast<IconId>(static_cast<int>(IconId::ChannelRed) +
                                        c),
                    10,
                    10);
                ch != 0) {
                // Dot vertically centered against the field height (Qt
                // centers grid cells vertically).
                ImGui::SetCursorScreenPos(
                    ImVec2(cell_x, row_pos.y + (field_h - dot_w) * 0.5f));
                ImGui::Image(
                    static_cast<ImTextureID>(static_cast<intptr_t>(ch)),
                    ImVec2(dot_w, dot_w));
            }
            ImGui::SetCursorScreenPos(
                ImVec2(cell_x + dot_w + dot_gap, row_pos.y));
            ImGui::SetNextItemWidth(field_w);
            if (c < channel_count) {
                float* v = (row == 0) ? &mins[c] : &maxs[c];
                // %g matches Qt's QString::number display ("255", not
                // "255.000").
                ImGui::InputFloat("##val", v, 0.0f, 0.0f, "%g");
                // Commit on focus-loss-after-edit (parity with Qt's
                // editingFinished) rather than on every keystroke, so
                // recomputing the contrast parameters doesn't happen
                // mid-typing.
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    buffer->compute_contrast_brightness_parameters();
                }
            } else {
                // Unused channel column: empty greyed field, Qt parity.
                std::array<char, 2> empty = {};
                ImGui::InputText("##val",
                                 empty.data(),
                                 empty.size(),
                                 ImGuiInputTextFlags_ReadOnly);
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        // Qt ac_reset_min / ac_reset_max at the row ends (fontello U+E808),
        // sized to the field height so the row reads as one line.
        ImGui::SetCursorScreenPos(
            ImVec2(row_pos.x + 4.0f * cell_pitch, row_pos.y));
        ImGui::PushID(100 + row);
        if (icon_button(kIconAcReset,
                        row == 0 ? "Reset minimum auto contrast levels to the "
                                   "lowest values found in the buffer"
                                 : "Reset maximum auto contrast levels to the "
                                   "largest values found in the buffer",
                        nullptr,
                        ImVec2(26.0f, field_h))) {
            if (row == 0) {
                buffer->recompute_min_color_values();
            } else {
                buffer->recompute_max_color_values();
            }
            buffer->compute_contrast_brightness_parameters();
        }
        ImGui::PopID();
    }
    ImGui::EndGroup();

    ImGui::Unindent(10.0f);
    ImGui::EndDisabled();
}

} // namespace oid::host
