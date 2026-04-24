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

#include "auto_contrast_controller.h"

#include <functional>
#include <optional>
#include <ranges>
#include <span>

#include <QLineEdit>

#include "ui/main_window/main_window.h"
#include "visualization/components/buffer.h"
#include "visualization/game_object.h"


namespace oid
{

namespace
{

void enable_inputs(const std::initializer_list<QLineEdit*>& inputs)
{
    for (auto& input : inputs) {
        input->setEnabled(true);
    }
}


void disable_inputs(const std::initializer_list<QLineEdit*>& inputs)
{
    for (auto& input : inputs) {
        input->setEnabled(false);
        input->setText("");
    }
}

void update_channel_labels(const Buffer& buffer,
                           const std::span<const float> values,
                           QLineEdit* c1,
                           QLineEdit* c2,
                           QLineEdit* c3,
                           QLineEdit* c4)
{
    // In single-channel mode, show only one input with the selected channel's
    // value
    if (buffer.get_display_channel_mode() == 1) {
        if (const auto selected_index = buffer.get_selected_channel_index();
            selected_index < static_cast<int>(values.size())) {
            c1->setText(QString::number(values[selected_index]));
        } else {
            c1->setText("0");
        }
        disable_inputs({c2, c3, c4});
        return;
    }

    // Normal multi-channel mode
    c1->setText(QString::number(values[0]));

    if (buffer.channels == 4) {
        enable_inputs({c2, c3, c4});
        c2->setText(QString::number(values[1]));
        c3->setText(QString::number(values[2]));
        c4->setText(QString::number(values[3]));
    } else if (buffer.channels == 3) {
        enable_inputs({c2, c3});
        c4->setEnabled(false);
        c2->setText(QString::number(values[1]));
        c3->setText(QString::number(values[2]));
    } else if (buffer.channels == 2) {
        c2->setEnabled(true);
        disable_inputs({c3, c4});
        c2->setText(QString::number(values[1]));
    } else {
        disable_inputs({c2, c3, c4});
    }
}

} // anonymous namespace


AutoContrastController::AutoContrastController(const Dependencies& deps)
    : deps_{deps}
{
}


namespace
{

std::optional<std::reference_wrapper<Buffer>>
get_buffer_component(const BufferData& buffer_data)
{
    const auto stage = buffer_data.currently_selected_stage.lock();
    if (!stage) {
        return std::nullopt;
    }
    const auto buffer_obj = stage->get_game_object("buffer");
    if (!buffer_obj.has_value()) {
        return std::nullopt;
    }
    const auto buffer_opt =
        buffer_obj->get().get_component<Buffer>("buffer_component");
    if (!buffer_opt.has_value()) {
        return std::nullopt;
    }
    return std::ref(buffer_opt->get());
}

} // anonymous namespace


void AutoContrastController::reset_min_labels() const
{
    const auto lock       = std::unique_lock{deps_.ui_mutex};
    const auto buffer_opt = get_buffer_component(deps_.buffer_data);
    if (!buffer_opt.has_value()) {
        return;
    }
    auto& buffer      = buffer_opt->get();
    const auto ac_min = buffer.min_buffer_values();

    update_channel_labels(buffer,
                          ac_min,
                          deps_.ui_components.ui->ac_c1_min,
                          deps_.ui_components.ui->ac_c2_min,
                          deps_.ui_components.ui->ac_c3_min,
                          deps_.ui_components.ui->ac_c4_min);
}


void AutoContrastController::reset_max_labels() const
{
    const auto lock       = std::unique_lock{deps_.ui_mutex};
    const auto buffer_opt = get_buffer_component(deps_.buffer_data);
    if (!buffer_opt.has_value()) {
        return;
    }
    auto& buffer      = buffer_opt->get();
    const auto ac_max = buffer.max_buffer_values();

    update_channel_labels(buffer,
                          ac_max,
                          deps_.ui_components.ui->ac_c1_max,
                          deps_.ui_components.ui->ac_c2_max,
                          deps_.ui_components.ui->ac_c3_max,
                          deps_.ui_components.ui->ac_c4_max);
}


void AutoContrastController::ac_c1_min_update()
{
    set_ac_min_value(0, deps_.ui_components.ui->ac_c1_min->text().toFloat());
}


void AutoContrastController::ac_c2_min_update()
{
    set_ac_min_value(1, deps_.ui_components.ui->ac_c2_min->text().toFloat());
}


void AutoContrastController::ac_c3_min_update()
{
    set_ac_min_value(2, deps_.ui_components.ui->ac_c3_min->text().toFloat());
}


void AutoContrastController::ac_c4_min_update()
{
    set_ac_min_value(3, deps_.ui_components.ui->ac_c4_min->text().toFloat());
}


void AutoContrastController::ac_c1_max_update()
{
    set_ac_max_value(0, deps_.ui_components.ui->ac_c1_max->text().toFloat());
}


void AutoContrastController::ac_c2_max_update()
{
    set_ac_max_value(1, deps_.ui_components.ui->ac_c2_max->text().toFloat());
}


void AutoContrastController::ac_c3_max_update()
{
    set_ac_max_value(2, deps_.ui_components.ui->ac_c3_max->text().toFloat());
}


void AutoContrastController::ac_c4_max_update()
{
    set_ac_max_value(3, deps_.ui_components.ui->ac_c4_max->text().toFloat());
}


void AutoContrastController::ac_min_reset()
{
    reset_color_values(true);
}


void AutoContrastController::ac_max_reset()
{
    reset_color_values(false);
}


void AutoContrastController::ac_toggle(const bool is_checked)
{
    const auto lock        = std::unique_lock{deps_.ui_mutex};
    deps_.state.ac_enabled = is_checked;
    for (const auto& stage : deps_.buffer_data.stages | std::views::values) {
        stage->set_contrast_enabled(deps_.state.ac_enabled);
    }

    deps_.state.request_render_update = true;
    deps_.state.request_icons_update  = true;
}


void AutoContrastController::set_ac_min_value(const int idx, const float value)
{
    set_ac_value(idx, value, true);
}


void AutoContrastController::set_ac_max_value(const int idx, const float value)
{
    set_ac_value(idx, value, false);
}


void AutoContrastController::set_ac_value(const int idx,
                                          const float value,
                                          const bool is_min)
{
    const auto lock       = std::unique_lock{deps_.ui_mutex};
    const auto buffer_opt = get_buffer_component(deps_.buffer_data);
    if (!buffer_opt.has_value()) {
        return;
    }
    auto& buffer = buffer_opt->get();

    if (is_min) {
        buffer.min_buffer_values()[idx] = value;
    } else {
        buffer.max_buffer_values()[idx] = value;
    }
    buffer.compute_contrast_brightness_parameters();

    deps_.state.request_render_update = true;
    deps_.state.request_icons_update  = true;
}


void AutoContrastController::reset_color_values(const bool is_min)
{
    const auto lock       = std::unique_lock{deps_.ui_mutex};
    const auto buffer_opt = get_buffer_component(deps_.buffer_data);
    if (!buffer_opt.has_value()) {
        return;
    }
    auto& buffer = buffer_opt->get();

    if (is_min) {
        buffer.recompute_min_color_values();
        reset_min_labels();
    } else {
        buffer.recompute_max_color_values();
        reset_max_labels();
    }
    buffer.compute_contrast_brightness_parameters();

    deps_.state.request_render_update = true;
    deps_.state.request_icons_update  = true;
}

} // namespace oid
