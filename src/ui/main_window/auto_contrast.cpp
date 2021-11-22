/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 OpenImageDebugger contributors
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

#include <QLineEdit>

#include "main_window.h"

#include "ui_main_window.h"
#include "visualization/game_object.h"


using namespace std;


void enable_inputs(const initializer_list<QLineEdit*>& inputs)
{
    for (auto& input : inputs) {
        input->setEnabled(true);
    }
}


void disable_inputs(const initializer_list<QLineEdit*>& inputs)
{
    for (auto& input : inputs) {
        input->setEnabled(false);
        input->setText("");
    }
}


void MainWindow::reset_ac_min_labels()
{
    GameObject* buffer_obj = currently_selected_stage_->get_game_object("buffer");
    Buffer* buffer = buffer_obj->get_component<Buffer>("buffer_component");
    float* ac_min  = buffer->min_buffer_values();

    ui_->ac_c1_min->setText(QString::number(ac_min[0]));

    if (buffer->channels == 4) {
        enable_inputs({ui_->ac_c2_min, ui_->ac_c3_min, ui_->ac_c4_min});

        ui_->ac_c2_min->setText(QString::number(ac_min[1]));
        ui_->ac_c3_min->setText(QString::number(ac_min[2]));
        ui_->ac_c4_min->setText(QString::number(ac_min[3]));
    } else if (buffer->channels == 3) {
        enable_inputs({ui_->ac_c2_min, ui_->ac_c3_min});

        ui_->ac_c4_min->setEnabled(false);

        ui_->ac_c2_min->setText(QString::number(ac_min[1]));
        ui_->ac_c3_min->setText(QString::number(ac_min[2]));
    } else if (buffer->channels == 2) {
        ui_->ac_c2_min->setEnabled(true);

        disable_inputs({ui_->ac_c3_min, ui_->ac_c4_min});

        ui_->ac_c2_min->setText(QString::number(ac_min[1]));
    } else {
        disable_inputs(
            {ui_->ac_c2_min, ui_->ac_c3_min, ui_->ac_c4_min});
    }
}


void MainWindow::reset_ac_max_labels()
{
    GameObject* buffer_obj = currently_selected_stage_->get_game_object("buffer");
    Buffer* buffer = buffer_obj->get_component<Buffer>("buffer_component");
    float* ac_max  = buffer->max_buffer_values();

    ui_->ac_c1_max->setText(QString::number(ac_max[0]));
    if (buffer->channels == 4) {
        enable_inputs({ui_->ac_c2_max, ui_->ac_c3_max, ui_->ac_c4_max});

        ui_->ac_c2_max->setText(QString::number(ac_max[1]));
        ui_->ac_c3_max->setText(QString::number(ac_max[2]));
        ui_->ac_c4_max->setText(QString::number(ac_max[3]));
    } else if (buffer->channels == 3) {
        enable_inputs({ui_->ac_c2_max, ui_->ac_c3_max});

        ui_->ac_c4_max->setEnabled(false);

        ui_->ac_c2_max->setText(QString::number(ac_max[1]));
        ui_->ac_c3_max->setText(QString::number(ac_max[2]));
    } else if (buffer->channels == 2) {
        ui_->ac_c2_max->setEnabled(true);

        disable_inputs({ui_->ac_c3_max, ui_->ac_c4_max});

        ui_->ac_c2_max->setText(QString::number(ac_max[1]));
    } else {
        disable_inputs(
            {ui_->ac_c2_max, ui_->ac_c3_max, ui_->ac_c4_max});
    }
}


void MainWindow::ac_c1_min_update()
{
    set_ac_min_value(0, ui_->ac_c1_min->text().toFloat());
}


void MainWindow::ac_c2_min_update()
{
    set_ac_min_value(1, ui_->ac_c2_min->text().toFloat());
}


void MainWindow::ac_c3_min_update()
{
    set_ac_min_value(2, ui_->ac_c3_min->text().toFloat());
}


void MainWindow::ac_c4_min_update()
{
    set_ac_min_value(3, ui_->ac_c4_min->text().toFloat());
}


void MainWindow::ac_c1_max_update()
{
    set_ac_max_value(0, ui_->ac_c1_max->text().toFloat());
}


void MainWindow::ac_c2_max_update()
{
    set_ac_max_value(1, ui_->ac_c2_max->text().toFloat());
}


void MainWindow::ac_c3_max_update()
{
    set_ac_max_value(2, ui_->ac_c3_max->text().toFloat());
}


void MainWindow::ac_c4_max_update()
{
    set_ac_max_value(3, ui_->ac_c4_max->text().toFloat());
}


void MainWindow::ac_min_reset()
{
    if (currently_selected_stage_ != nullptr) {
        GameObject* buffer_obj =
            currently_selected_stage_->get_game_object("buffer");
        Buffer* buff = buffer_obj->get_component<Buffer>("buffer_component");
        buff->recompute_min_color_values();
        buff->compute_contrast_brightness_parameters();

        // Update inputs
        reset_ac_min_labels();

        request_render_update_ = true;
        request_icons_update_ = true;
    }
}


void MainWindow::ac_max_reset()
{
    if (currently_selected_stage_ != nullptr) {
        GameObject* buffer_obj =
            currently_selected_stage_->get_game_object("buffer");
        Buffer* buff = buffer_obj->get_component<Buffer>("buffer_component");
        buff->recompute_max_color_values();
        buff->compute_contrast_brightness_parameters();

        // Update inputs
        reset_ac_max_labels();

        request_render_update_ = true;
        request_icons_update_ = true;
    }
}


void MainWindow::ac_toggle(bool is_checked)
{
    ac_enabled_ = is_checked;
    for (auto& stage : stages_)
        stage.second->contrast_enabled = ac_enabled_;

    request_render_update_ = true;
    request_icons_update_ = true;
}


void MainWindow::set_ac_min_value(int idx, float value)
{
    if (currently_selected_stage_ != nullptr) {
        GameObject* buffer_obj =
            currently_selected_stage_->get_game_object("buffer");
        Buffer* buff = buffer_obj->get_component<Buffer>("buffer_component");
        buff->min_buffer_values()[idx] = value;
        buff->compute_contrast_brightness_parameters();

        request_render_update_ = true;
        request_icons_update_ = true;
    }
}


void MainWindow::set_ac_max_value(int idx, float value)
{
    if (currently_selected_stage_ != nullptr) {
        GameObject* buffer_obj =
            currently_selected_stage_->get_game_object("buffer");
        Buffer* buff = buffer_obj->get_component<Buffer>("buffer_component");
        buff->max_buffer_values()[idx] = value;
        buff->compute_contrast_brightness_parameters();

        request_render_update_ = true;
        request_icons_update_ = true;
    }
}
