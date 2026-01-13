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

#include <ranges>

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

} // anonymous namespace


AutoContrastController::AutoContrastController(Dependencies deps)
    : deps_{deps}
{
}


void AutoContrastController::reset_min_labels() const
{
    const auto lock  = std::unique_lock{deps_.ui_mutex};
    const auto stage = deps_.buffer_data.currently_selected_stage.lock();
    if (!stage) {
        return;
    }
    const auto buffer_obj = stage->get_game_object("buffer");
    if (!buffer_obj.has_value()) {
        return;
    }
    const auto buffer_opt =
        buffer_obj->get().get_component<Buffer>("buffer_component");
    if (!buffer_opt.has_value()) {
        return;
    }
    auto& buffer      = buffer_opt->get();
    const auto ac_min = buffer.min_buffer_values();

    deps_.ui_components.ui->ac_c1_min->setText(QString::number(ac_min[0]));

    if (buffer.channels == 4) {
        enable_inputs({deps_.ui_components.ui->ac_c2_min,
                       deps_.ui_components.ui->ac_c3_min,
                       deps_.ui_components.ui->ac_c4_min});

        deps_.ui_components.ui->ac_c2_min->setText(QString::number(ac_min[1]));
        deps_.ui_components.ui->ac_c3_min->setText(QString::number(ac_min[2]));
        deps_.ui_components.ui->ac_c4_min->setText(QString::number(ac_min[3]));
    } else if (buffer.channels == 3) {
        enable_inputs({deps_.ui_components.ui->ac_c2_min,
                       deps_.ui_components.ui->ac_c3_min});

        deps_.ui_components.ui->ac_c4_min->setEnabled(false);

        deps_.ui_components.ui->ac_c2_min->setText(QString::number(ac_min[1]));
        deps_.ui_components.ui->ac_c3_min->setText(QString::number(ac_min[2]));
    } else if (buffer.channels == 2) {
        deps_.ui_components.ui->ac_c2_min->setEnabled(true);

        disable_inputs({deps_.ui_components.ui->ac_c3_min,
                        deps_.ui_components.ui->ac_c4_min});

        deps_.ui_components.ui->ac_c2_min->setText(QString::number(ac_min[1]));
    } else {
        disable_inputs({deps_.ui_components.ui->ac_c2_min,
                        deps_.ui_components.ui->ac_c3_min,
                        deps_.ui_components.ui->ac_c4_min});
    }
}


void AutoContrastController::reset_max_labels() const
{
    const auto lock  = std::unique_lock{deps_.ui_mutex};
    const auto stage = deps_.buffer_data.currently_selected_stage.lock();
    if (!stage) {
        return;
    }
    const auto buffer_obj = stage->get_game_object("buffer");
    if (!buffer_obj.has_value()) {
        return;
    }
    const auto buffer_opt =
        buffer_obj->get().get_component<Buffer>("buffer_component");
    if (!buffer_opt.has_value()) {
        return;
    }
    auto& buffer      = buffer_opt->get();
    const auto ac_max = buffer.max_buffer_values();

    deps_.ui_components.ui->ac_c1_max->setText(QString::number(ac_max[0]));
    if (buffer.channels == 4) {
        enable_inputs({deps_.ui_components.ui->ac_c2_max,
                       deps_.ui_components.ui->ac_c3_max,
                       deps_.ui_components.ui->ac_c4_max});

        deps_.ui_components.ui->ac_c2_max->setText(QString::number(ac_max[1]));
        deps_.ui_components.ui->ac_c3_max->setText(QString::number(ac_max[2]));
        deps_.ui_components.ui->ac_c4_max->setText(QString::number(ac_max[3]));
    } else if (buffer.channels == 3) {
        enable_inputs({deps_.ui_components.ui->ac_c2_max,
                       deps_.ui_components.ui->ac_c3_max});

        deps_.ui_components.ui->ac_c4_max->setEnabled(false);

        deps_.ui_components.ui->ac_c2_max->setText(QString::number(ac_max[1]));
        deps_.ui_components.ui->ac_c3_max->setText(QString::number(ac_max[2]));
    } else if (buffer.channels == 2) {
        deps_.ui_components.ui->ac_c2_max->setEnabled(true);

        disable_inputs({deps_.ui_components.ui->ac_c3_max,
                        deps_.ui_components.ui->ac_c4_max});

        deps_.ui_components.ui->ac_c2_max->setText(QString::number(ac_max[1]));
    } else {
        disable_inputs({deps_.ui_components.ui->ac_c2_max,
                        deps_.ui_components.ui->ac_c3_max,
                        deps_.ui_components.ui->ac_c4_max});
    }
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
    const auto lock  = std::unique_lock{deps_.ui_mutex};
    const auto stage = deps_.buffer_data.currently_selected_stage.lock();
    if (stage == nullptr) {
        return;
    }

    const auto buffer_obj = stage->get_game_object("buffer");
    if (!buffer_obj.has_value()) {
        return;
    }

    const auto buff_opt =
        buffer_obj->get().get_component<Buffer>("buffer_component");
    if (!buff_opt.has_value()) {
        return;
    }
    auto& buff = buff_opt->get();

    buff.recompute_min_color_values();
    buff.compute_contrast_brightness_parameters();

    reset_min_labels();

    deps_.state.request_render_update = true;
    deps_.state.request_icons_update  = true;
}


void AutoContrastController::ac_max_reset()
{
    const auto lock  = std::unique_lock{deps_.ui_mutex};
    const auto stage = deps_.buffer_data.currently_selected_stage.lock();
    if (stage == nullptr) {
        return;
    }

    const auto buffer_obj = stage->get_game_object("buffer");
    if (!buffer_obj.has_value()) {
        return;
    }

    const auto buff_opt =
        buffer_obj->get().get_component<Buffer>("buffer_component");
    if (!buff_opt.has_value()) {
        return;
    }
    auto& buff = buff_opt->get();

    buff.recompute_max_color_values();
    buff.compute_contrast_brightness_parameters();

    reset_max_labels();

    deps_.state.request_render_update = true;
    deps_.state.request_icons_update  = true;
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
    const auto lock  = std::unique_lock{deps_.ui_mutex};
    const auto stage = deps_.buffer_data.currently_selected_stage.lock();
    if (stage == nullptr) {
        return;
    }

    const auto buffer_obj = stage->get_game_object("buffer");
    if (!buffer_obj.has_value()) {
        return;
    }

    const auto buff_opt =
        buffer_obj->get().get_component<Buffer>("buffer_component");
    if (!buff_opt.has_value()) {
        return;
    }
    auto& buff = buff_opt->get();

    buff.min_buffer_values()[idx] = value;
    buff.compute_contrast_brightness_parameters();

    deps_.state.request_render_update = true;
    deps_.state.request_icons_update  = true;
}


void AutoContrastController::set_ac_max_value(const int idx, const float value)
{
    const auto lock  = std::unique_lock{deps_.ui_mutex};
    const auto stage = deps_.buffer_data.currently_selected_stage.lock();
    if (stage == nullptr) {
        return;
    }

    const auto buffer_obj = stage->get_game_object("buffer");
    if (!buffer_obj.has_value()) {
        return;
    }

    const auto buff_opt =
        buffer_obj->get().get_component<Buffer>("buffer_component");
    if (!buff_opt.has_value()) {
        return;
    }
    auto& buff = buff_opt->get();

    buff.max_buffer_values()[idx] = value;
    buff.compute_contrast_brightness_parameters();

    deps_.state.request_render_update = true;
    deps_.state.request_icons_update  = true;
}

} // namespace oid
