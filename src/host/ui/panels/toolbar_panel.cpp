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

#include "host/ui/panels/toolbar_panel.h"

#include <array>
#include <cstddef>
#include <numbers>
#include <string>

#include <imgui.h>

#include "host/ui/buffer_model.h"
#include "host/ui/panels/icon_button.h"
#include "host/ui/panels/panel_accessors.h"
#include "host/ui/stage_manager.h"
#include "host/ui/ui_state.h"
#include "host/ui_fonts.h"
#include "ipc/raw_data_decode.h"
#include "visualization/components/buffer.h"
#include "visualization/components/buffer_values.h"
#include "visualization/components/camera.h"
#include "visualization/stage.h"

namespace oid::host {

namespace {

// Fan-out helper: applies `fn` to the selected Stage normally, or to every
// buffer's Stage when link-views is on (parity with the Qt app's link-views,
// where a toolbar action on one view applies to all open views). Stages that
// haven't initialized yet (nullptr) are silently skipped rather than
// crashing the frame.
template <typename Fn>
void for_each_target(const UiState& ui,
                     StageManager& stages,
                     const BufferModel& model,
                     Fn&& fn) {
    if (ui.link_views()) {
        for (std::size_t i = 0; i < model.size(); ++i) {
            if (oid::Stage* s = stages.stage_for(i); s != nullptr) {
                fn(*s);
            }
        }
        return;
    }

    if (oid::Stage* s = stages.selected_stage(ui.selected()); s != nullptr) {
        fn(*s);
    }
}

// Combo entries, in order, exactly matching the legacy Qt frontend's
// PixelDisplayFormat/apply_format() (see tag legacy-qt).
constexpr std::array<const char*, 5> FORMAT_LABELS = {
    "BGRA", "RGBA", "channel0", "channel1", "channel2"};
constexpr int FORMAT_COUNT = static_cast<int>(FORMAT_LABELS.size());

enum class PixelFormat {
    BGRA = 0,
    RGBA = 1,
    CHANNEL0 = 2,
    CHANNEL1 = 3,
    CHANNEL2 = 4
};

// Applies the combo selection to `b`, mirroring the legacy Qt frontend's
// apply_format() (see tag legacy-qt) exactly, including its
// display-channel-mode / pixel-layout pairing.
void apply_format(oid::Buffer& b, int sel) {
    using enum PixelFormat;
    switch (sel) {
    case static_cast<int>(BGRA):
        b.set_display_channel_mode(-1);
        b.set_pixel_layout("bgra");
        break;
    case static_cast<int>(RGBA):
        b.set_display_channel_mode(-1);
        b.set_pixel_layout("rgba");
        break;
    case static_cast<int>(CHANNEL0):
        b.set_display_channel_mode(1);
        b.set_pixel_layout("rrra");
        break;
    case static_cast<int>(CHANNEL1):
        b.set_display_channel_mode(1);
        b.set_pixel_layout("ggga");
        break;
    case static_cast<int>(CHANNEL2):
        b.set_display_channel_mode(1);
        b.set_pixel_layout("bbba");
        break;
    default:
        break;
    }
}

// Derives the combo's current selection from the buffer's live state
// (display-channel-mode + pixel layout) every frame rather than caching a
// selection: with no cache, switching the Stage/Buffer this panel targets
// (e.g. clicking a different buffer in the list) always reflects that
// buffer's actual format instead of carrying over a stale index or
// requiring extra per-buffer bookkeeping.
int current_format_index(const oid::Buffer& b) {
    using enum PixelFormat;
    const std::string layout = b.get_pixel_layout();
    if (b.get_display_channel_mode() == 1) {
        if (layout == "ggga") {
            return static_cast<int>(CHANNEL1);
        }
        if (layout == "bbba") {
            return static_cast<int>(CHANNEL2);
        }
        return static_cast<int>(CHANNEL0); // "rrra", or any other
                                           // single-channel layout
    }
    return static_cast<int>(layout == "rgba" ? RGBA : BGRA);
}

// Auto-contrast is global UI state. Sync the selected Stage to it every
// frame (not only on click): a Stage's own contrast flag defaults to off
// and is otherwise never told about the toggle, so without this the
// toggle and the actual render disagree at startup and after buffer
// switches / newly-created Stages. AC is global, so syncing the currently
// displayed Stage is sufficient -- any buffer shows the toggle's state
// when viewed.
void sync_selected_stage_contrast(const UiState& ui, StageManager& stages) {
    if (oid::Stage* s = stages.selected_stage(ui.selected()); s != nullptr) {
        s->set_contrast_enabled(ui.contrast_enabled());
    }
}

// 1. acEdit / 2. acToggle: shows/hides the min/max intensity editor and
// enables/disables contrast modifications. Plain UI-state toggles -- not
// gated on selection.
void draw_auto_contrast_controls(UiState& ui) {
    {
        bool ac_editor_visible = ui.ac_editor_visible();
        if (icon_button(ICON_AC_EDIT,
                        "Toggle min and max intensity editor",
                        &ac_editor_visible)) {
            ui.set_ac_editor_visible(!ui.ac_editor_visible());
        }
    }
    ImGui::SameLine();

    {
        bool contrast_enabled = ui.contrast_enabled();
        if (icon_button(ICON_AC_TOGGLE,
                        "Toggle Contrast Modifications",
                        &contrast_enabled)) {
            ui.set_contrast_enabled(!ui.contrast_enabled());
        }
    }
    ImGui::SameLine();
}

// 3. reposition_buffer (recenter). Gated on selection.
void draw_recenter_button(const UiState& ui,
                          StageManager& stages,
                          const BufferModel& model,
                          bool has_selection) {
    ImGui::BeginDisabled(!has_selection);

    if (icon_button(ICON_RECENTER, "Reposition buffer to fit window")) {
        for_each_target(ui, stages, model, [](oid::Stage& s) {
            if (oid::Camera* cam = camera_of(s); cam != nullptr) {
                cam->recenter_camera();
            }
        });
    }
    ImGui::SameLine();

    ImGui::EndDisabled();
}

// 4. linkViewsToggle. Plain UI-state toggle -- not gated on selection.
void draw_link_views_toggle(UiState& ui) {
    if (const bool link_views = ui.link_views(); icon_button(
            ICON_LINK_VIEWS, "Move all buffers simultaneously", &link_views)) {
        ui.set_link_views(!ui.link_views());
    }
    ImGui::SameLine();
}

// 5. rotate_90_ccw / 6. rotate_90_cw. Gated on selection.
void draw_rotation_buttons(const UiState& ui,
                           StageManager& stages,
                           const BufferModel& model,
                           bool has_selection) {
    ImGui::BeginDisabled(!has_selection);

    if (icon_button(ICON_ROTATE_CCW, "Rotate 90\xC2\xB0 counterclockwise")) {
        for_each_target(ui, stages, model, [](oid::Stage& s) {
            if (oid::Buffer* buf = buffer_of(s); buf != nullptr) {
                buf->rotate(-90.0f * std::numbers::pi_v<float> / 180.0f);
            }
        });
    }
    ImGui::SameLine();

    if (icon_button(ICON_ROTATE_CW, "Rotate 90\xC2\xB0 clockwise")) {
        for_each_target(ui, stages, model, [](oid::Stage& s) {
            if (oid::Buffer* buf = buffer_of(s); buf != nullptr) {
                buf->rotate(90.0f * std::numbers::pi_v<float> / 180.0f);
            }
        });
    }
    ImGui::SameLine();

    ImGui::EndDisabled();
}

// 7. go_to_pixel. Gated on selection.
void draw_goto_button(bool has_selection, bool& goto_open) {
    ImGui::BeginDisabled(!has_selection);

    if (icon_button(ICON_GO_TO, "Go to pixel position")) {
        goto_open = true;
    }
    ImGui::SameLine();

    ImGui::EndDisabled();
}

// 8. decrease_float_precision / 9. increase_float_precision. Gated on
// selection AND float type (parity with the legacy Qt frontend's
// per-selection updates; see tag legacy-qt).
void draw_precision_buttons(const UiState& ui,
                            StageManager& stages,
                            const BufferModel& model,
                            bool has_selection) {
    const bool float_selected =
        has_selection &&
        (model.at(ui.selected()).type == oid::BufferType::FLOAT32 ||
         model.at(ui.selected()).type == oid::BufferType::FLOAT64);
    ImGui::BeginDisabled(!float_selected);

    if (icon_button(ICON_PRECISION_DOWN, "Decrease float precision")) {
        for_each_target(ui, stages, model, [](oid::Stage& s) {
            if (oid::BufferValues* vals = values_of(s); vals != nullptr) {
                vals->decrease_float_precision();
            }
        });
    }
    ImGui::SameLine();

    if (icon_button(ICON_PRECISION_UP, "Increase float precision")) {
        for_each_target(ui, stages, model, [](oid::Stage& s) {
            if (oid::BufferValues* vals = values_of(s); vals != nullptr) {
                vals->increase_float_precision();
            }
        });
    }
    ImGui::SameLine();

    ImGui::EndDisabled();
}

// Format combo: operates on the selected stage's buffer only (the legacy
// Qt frontend's selector acted on the selected buffer only via
// apply_format(); see tag legacy-qt), no link-views fan-out.
void draw_format_combo(const UiState& ui, StageManager& stages) {
    ImGui::TextUnformatted("Format:");
    ImGui::SameLine();

    oid::Stage* selected_stage = stages.selected_stage(ui.selected());
    oid::Buffer* selected_buffer =
        selected_stage != nullptr ? buffer_of(*selected_stage) : nullptr;
    const bool format_disabled =
        selected_buffer == nullptr || selected_buffer->channels() < 3;

    ImGui::BeginDisabled(format_disabled);
    ImGui::SetNextItemWidth(100.0f);
    if (int current = selected_buffer != nullptr
                          ? current_format_index(*selected_buffer)
                          : 0;
        ImGui::Combo(
            "##pixel_format", &current, FORMAT_LABELS.data(), FORMAT_COUNT) &&
        selected_buffer != nullptr) {
        apply_format(*selected_buffer, current);
    }
    ImGui::SetItemTooltip("Select pixel channel layout");
    ImGui::EndDisabled();
}

} // namespace

void draw_toolbar(UiState& ui,
                  StageManager& stages,
                  const BufferModel& model,
                  bool& goto_open) {
    const bool has_selection = ui.has_selection();

    sync_selected_stage_contrast(ui, stages);
    draw_auto_contrast_controls(ui);
    draw_recenter_button(ui, stages, model, has_selection);
    draw_link_views_toggle(ui);
    draw_rotation_buttons(ui, stages, model, has_selection);
    draw_goto_button(has_selection, goto_open);
    draw_precision_buttons(ui, stages, model, has_selection);
    draw_format_combo(ui, stages);
}

} // namespace oid::host
