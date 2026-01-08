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

#include "main_window.h"

#include <ranges>

#include <QLineEdit>

#include "ui_main_window.h"
#include "visualization/components/buffer.h"
#include "visualization/game_object.h"


namespace oid
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


void MainWindow::reset_ac_min_labels() const
{
    const auto buffer_obj =
        buffer_data_.currently_selected_stage->get_game_object("buffer");
    const auto buffer = buffer_obj->get_component<Buffer>("buffer_component");
    const auto ac_min = buffer->min_buffer_values();

    ui_components_.ui->ac_c1_min->setText(QString::number(ac_min[0]));

    if (buffer->channels == 4) {
        enable_inputs({ui_components_.ui->ac_c2_min,
                       ui_components_.ui->ac_c3_min,
                       ui_components_.ui->ac_c4_min});

        ui_components_.ui->ac_c2_min->setText(QString::number(ac_min[1]));
        ui_components_.ui->ac_c3_min->setText(QString::number(ac_min[2]));
        ui_components_.ui->ac_c4_min->setText(QString::number(ac_min[3]));
    } else if (buffer->channels == 3) {
        enable_inputs(
            {ui_components_.ui->ac_c2_min, ui_components_.ui->ac_c3_min});

        ui_components_.ui->ac_c4_min->setEnabled(false);

        ui_components_.ui->ac_c2_min->setText(QString::number(ac_min[1]));
        ui_components_.ui->ac_c3_min->setText(QString::number(ac_min[2]));
    } else if (buffer->channels == 2) {
        ui_components_.ui->ac_c2_min->setEnabled(true);

        disable_inputs(
            {ui_components_.ui->ac_c3_min, ui_components_.ui->ac_c4_min});

        ui_components_.ui->ac_c2_min->setText(QString::number(ac_min[1]));
    } else {
        disable_inputs({ui_components_.ui->ac_c2_min,
                        ui_components_.ui->ac_c3_min,
                        ui_components_.ui->ac_c4_min});
    }
}


void MainWindow::reset_ac_max_labels() const
{
    const auto buffer_obj =
        buffer_data_.currently_selected_stage->get_game_object("buffer");
    const auto buffer = buffer_obj->get_component<Buffer>("buffer_component");
    const auto ac_max = buffer->max_buffer_values();

    ui_components_.ui->ac_c1_max->setText(QString::number(ac_max[0]));
    if (buffer->channels == 4) {
        enable_inputs({ui_components_.ui->ac_c2_max,
                       ui_components_.ui->ac_c3_max,
                       ui_components_.ui->ac_c4_max});

        ui_components_.ui->ac_c2_max->setText(QString::number(ac_max[1]));
        ui_components_.ui->ac_c3_max->setText(QString::number(ac_max[2]));
        ui_components_.ui->ac_c4_max->setText(QString::number(ac_max[3]));
    } else if (buffer->channels == 3) {
        enable_inputs(
            {ui_components_.ui->ac_c2_max, ui_components_.ui->ac_c3_max});

        ui_components_.ui->ac_c4_max->setEnabled(false);

        ui_components_.ui->ac_c2_max->setText(QString::number(ac_max[1]));
        ui_components_.ui->ac_c3_max->setText(QString::number(ac_max[2]));
    } else if (buffer->channels == 2) {
        ui_components_.ui->ac_c2_max->setEnabled(true);

        disable_inputs(
            {ui_components_.ui->ac_c3_max, ui_components_.ui->ac_c4_max});

        ui_components_.ui->ac_c2_max->setText(QString::number(ac_max[1]));
    } else {
        disable_inputs({ui_components_.ui->ac_c2_max,
                        ui_components_.ui->ac_c3_max,
                        ui_components_.ui->ac_c4_max});
    }
}


void MainWindow::ac_c1_min_update()
{
    set_ac_min_value(0, ui_components_.ui->ac_c1_min->text().toFloat());
}


void MainWindow::ac_c2_min_update()
{
    set_ac_min_value(1, ui_components_.ui->ac_c2_min->text().toFloat());
}


void MainWindow::ac_c3_min_update()
{
    set_ac_min_value(2, ui_components_.ui->ac_c3_min->text().toFloat());
}


void MainWindow::ac_c4_min_update()
{
    set_ac_min_value(3, ui_components_.ui->ac_c4_min->text().toFloat());
}


void MainWindow::ac_c1_max_update()
{
    set_ac_max_value(0, ui_components_.ui->ac_c1_max->text().toFloat());
}


void MainWindow::ac_c2_max_update()
{
    set_ac_max_value(1, ui_components_.ui->ac_c2_max->text().toFloat());
}


void MainWindow::ac_c3_max_update()
{
    set_ac_max_value(2, ui_components_.ui->ac_c3_max->text().toFloat());
}


void MainWindow::ac_c4_max_update()
{
    set_ac_max_value(3, ui_components_.ui->ac_c4_max->text().toFloat());
}


void MainWindow::ac_min_reset()
{
    if (buffer_data_.currently_selected_stage != nullptr) {
        const auto buffer_obj =
            buffer_data_.currently_selected_stage->get_game_object("buffer");
        const auto buff = buffer_obj->get_component<Buffer>("buffer_component");
        buff->recompute_min_color_values();
        buff->compute_contrast_brightness_parameters();

        // Update inputs
        reset_ac_min_labels();

        state_.request_render_update = true;
        state_.request_icons_update  = true;
    }
}


void MainWindow::ac_max_reset()
{
    if (buffer_data_.currently_selected_stage != nullptr) {
        const auto buffer_obj =
            buffer_data_.currently_selected_stage->get_game_object("buffer");
        const auto buff = buffer_obj->get_component<Buffer>("buffer_component");
        buff->recompute_max_color_values();
        buff->compute_contrast_brightness_parameters();

        // Update inputs
        reset_ac_max_labels();

        state_.request_render_update = true;
        state_.request_icons_update  = true;
    }
}


void MainWindow::ac_toggle(const bool is_checked)
{
    state_.ac_enabled = is_checked;
    for (const auto& stage : buffer_data_.stages | std::views::values) {
        stage->contrast_enabled = state_.ac_enabled;
    }

    state_.request_render_update = true;
    state_.request_icons_update  = true;
}


void MainWindow::set_ac_min_value(const int idx, const float value)
{
    if (buffer_data_.currently_selected_stage != nullptr) {
        const auto buffer_obj =
            buffer_data_.currently_selected_stage->get_game_object("buffer");
        const auto buff = buffer_obj->get_component<Buffer>("buffer_component");
        buff->min_buffer_values()[idx] = value;
        buff->compute_contrast_brightness_parameters();

        state_.request_render_update = true;
        state_.request_icons_update  = true;
    }
}


void MainWindow::set_ac_max_value(const int idx, const float value)
{
    if (buffer_data_.currently_selected_stage != nullptr) {
        const auto buffer_obj =
            buffer_data_.currently_selected_stage->get_game_object("buffer");
        const auto buff = buffer_obj->get_component<Buffer>("buffer_component");
        buff->max_buffer_values()[idx] = value;
        buff->compute_contrast_brightness_parameters();

        state_.request_render_update = true;
        state_.request_icons_update  = true;
    }
}

} // namespace oid
