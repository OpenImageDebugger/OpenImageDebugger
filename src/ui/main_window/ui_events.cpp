/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2024 OpenImageDebugger contributors
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

#include <QFileDialog>
#include <QtMath> // for portable definition of M_PI

#include "io/buffer_exporter.h"
#include "ui_main_window.h"
#include "visualization/components/buffer_values.h"
#include "visualization/components/camera.h"
#include "visualization/game_object.h"

using namespace std;

namespace oid
{

void MainWindow::resize_callback(const int w, const int h) const
{
    for (const auto& [_, stage] : stages_) {
        stage->resize_callback(w, h);
    }

    go_to_widget_->move(ui_->bufferPreview->width() - go_to_widget_->width(),
                        ui_->bufferPreview->height() - go_to_widget_->height());
}


void MainWindow::scroll_callback(const float delta)
{
    if (link_views_enabled_) {
        for (const auto& [_, stage] : stages_) {
            stage->scroll_callback(delta);
        }
    } else if (currently_selected_stage_ != nullptr) {
        currently_selected_stage_->scroll_callback(delta);
    }

    update_status_bar();

#if defined(Q_OS_DARWIN)
    ui_->bufferPreview->update();
#endif
    request_render_update_ = true;
}


void MainWindow::mouse_drag_event(const int mouse_x, const int mouse_y)
{
    const QPoint virtual_motion(mouse_x, mouse_y);

    if (link_views_enabled_) {
        for (const auto& [_, stage] : stages_) {
            stage->mouse_drag_event(virtual_motion.x(), virtual_motion.y());
        }
    } else if (currently_selected_stage_ != nullptr) {
        currently_selected_stage_->mouse_drag_event(virtual_motion.x(),
                                                    virtual_motion.y());
    }

    request_render_update_ = true;
}


void MainWindow::mouse_move_event(int, int) const
{
    update_status_bar();
}


void MainWindow::resizeEvent(QResizeEvent*)
{
    persist_settings_deferred();
}


void MainWindow::moveEvent(QMoveEvent*)
{
    persist_settings_deferred();
}


void MainWindow::closeEvent(QCloseEvent*)
{
    is_window_ready_ = false;
    persist_settings_deferred();
}


bool MainWindow::eventFilter(QObject* target, QEvent* event)
{
    KeyboardState::update_keyboard_state(event);

    if (event->type() == QEvent::KeyPress) {
        const auto* key_event = dynamic_cast<QKeyEvent*>(event);

        EventProcessCode event_intercepted = EventProcessCode::IGNORED;

        if (link_views_enabled_) {
            for (const auto& [_, stage] : stages_) {
                const EventProcessCode event_intercepted_stage =
                    stage->key_press_event(key_event->key());

                if (event_intercepted_stage == EventProcessCode::INTERCEPTED) {
                    event_intercepted = EventProcessCode::INTERCEPTED;
                }
            }
        } else if (currently_selected_stage_ != nullptr) {
            event_intercepted =
                currently_selected_stage_->key_press_event(key_event->key());
        }

        if (event_intercepted == EventProcessCode::INTERCEPTED) {
            request_render_update_ = true;
            update_status_bar();

            event->accept();
            return true;
        }

        return QObject::eventFilter(target, event);
    }

    return false;
}


void MainWindow::recenter_buffer()
{
    if (link_views_enabled_) {
        for (const auto& [_, stage] : stages_) {
            GameObject* cam_obj = stage->get_game_object("camera");
            auto* cam = cam_obj->get_component<Camera>("camera_component");
            cam->recenter_camera();
        }
    } else {
        if (currently_selected_stage_ != nullptr) {
            GameObject* cam_obj =
                currently_selected_stage_->get_game_object("camera");
            auto* cam = cam_obj->get_component<Camera>("camera_component");
            cam->recenter_camera();
        }
    }

    request_render_update_ = true;
}


void MainWindow::link_views_toggle()
{
    link_views_enabled_ = !link_views_enabled_;
}

void MainWindow::shift_precision_left()
{
    const auto shift_precision_left = [](Stage* stage) {
        GameObject* buffer_obj = stage->get_game_object("buffer");
        auto* buffer_comp =
            buffer_obj->get_component<BufferValues>("text_component");

        buffer_comp->shift_precision_left();
    };

    if (link_views_enabled_) {
        for (auto& [_, stage] : stages_) {
            shift_precision_left(stage.get());
        }
    } else {
        if (currently_selected_stage_ != nullptr) {
            shift_precision_left(currently_selected_stage_);
        }
    }

    request_render_update_ = true;
}

void MainWindow::shift_precision_right()
{
    const auto shift_precision_right = [](Stage* stage) {
        GameObject* buffer_obj = stage->get_game_object("buffer");
        auto* buffer_comp =
            buffer_obj->get_component<BufferValues>("text_component");

        buffer_comp->shift_precision_right();
    };

    if (link_views_enabled_) {
        for (auto& [_, stage] : stages_) {
            shift_precision_right(stage.get());
        }
    } else {
        if (currently_selected_stage_ != nullptr) {
            shift_precision_right(currently_selected_stage_);
        }
    }

    request_render_update_ = true;
}

void MainWindow::update_shift_precision()
{
    if (currently_selected_stage_ != nullptr) {
        GameObject* buffer_obj =
            currently_selected_stage_->get_game_object("buffer");
        const auto* buffer =
            buffer_obj->get_component<Buffer>("buffer_component");

        if ((BufferType::Float32 == buffer->type) ||
            (BufferType::Float64 == buffer->type)) {
            ui_->shift_precision_left->setEnabled(true);
            ui_->shift_precision_right->setEnabled(true);
        } else {
            ui_->shift_precision_left->setEnabled(false);
            ui_->shift_precision_right->setEnabled(false);
        }
    } else {
        ui_->shift_precision_left->setEnabled(false);
        ui_->shift_precision_right->setEnabled(false);
    }
}

void MainWindow::rotate_90_cw()
{
    const auto request_90_cw_rotation = [](Stage* stage) {
        GameObject* buffer_obj = stage->get_game_object("buffer");
        auto* buffer_comp =
            buffer_obj->get_component<Buffer>("buffer_component");

        buffer_comp->rotate(90.0f * static_cast<float>(M_PI) / 180.0f);
    };

    if (link_views_enabled_) {
        for (auto& [_, stage] : stages_) {
            request_90_cw_rotation(stage.get());
        }
    } else {
        if (currently_selected_stage_ != nullptr) {
            request_90_cw_rotation(currently_selected_stage_);
        }
    }

    request_render_update_ = true;
}


void MainWindow::rotate_90_ccw()
{
    const auto request_90_ccw_rotation = [](Stage* stage) {
        GameObject* buffer_obj = stage->get_game_object("buffer");
        auto* buffer_comp =
            buffer_obj->get_component<Buffer>("buffer_component");

        buffer_comp->rotate(-90.0f * static_cast<float>(M_PI) / 180.0f);
    };

    if (link_views_enabled_) {
        for (auto& [_, stage] : stages_) {
            request_90_ccw_rotation(stage.get());
        }
    } else {
        if (currently_selected_stage_ != nullptr) {
            request_90_ccw_rotation(currently_selected_stage_);
        }
    }

    request_render_update_ = true;
}


void MainWindow::buffer_selected(QListWidgetItem* item)
{
    if (item == nullptr) {
        return;
    }

    const auto stage =
        stages_.find(item->data(Qt::UserRole).toString().toStdString());
    if (stage != stages_.end()) {
        set_currently_selected_stage(stage->second.get());
        reset_ac_min_labels();
        reset_ac_max_labels();
        update_shift_precision();
        update_status_bar();
    }
}


void MainWindow::remove_selected_buffer()
{
    if (ui_->imageList->count() > 0 && currently_selected_stage_ != nullptr) {
        const QListWidgetItem* removed_item =
            ui_->imageList->takeItem(ui_->imageList->currentRow());
        const string buffer_name =
            removed_item->data(Qt::UserRole).toString().toStdString();
        stages_.erase(buffer_name);
        held_buffers_.erase(buffer_name);
        delete removed_item;

        removed_buffer_names_.insert(buffer_name);

        if (stages_.empty()) {
            set_currently_selected_stage(nullptr);
            update_shift_precision();
        }

        persist_settings_deferred();
    }
}


void MainWindow::symbol_selected()
{
    const QByteArray symbol_name_qba = ui_->symbolList->text().toLocal8Bit();
    const char* symbol_name          = symbol_name_qba.constData();
    if (ui_->symbolList->text().length() > 0) {
        request_plot_buffer(symbol_name);
        // Clear symbol input
        ui_->symbolList->setText("");
    }
}


void MainWindow::symbol_completed(const QString& str)
{
    if (str.length() > 0) {
        const QByteArray symbol_name_qba = str.toLocal8Bit();
        request_plot_buffer(symbol_name_qba.constData());
        // Clear symbol input
        ui_->symbolList->setText("");
        ui_->symbolList->clearFocus();
    }
}


void MainWindow::export_buffer()
{
    const auto sender_action(dynamic_cast<QAction*>(sender()));

    const auto stage =
        stages_.find(sender_action->data().toString().toStdString())->second;

    GameObject* buffer_obj = stage->get_game_object("buffer");
    const auto* component =
        buffer_obj->get_component<Buffer>("buffer_component");

    QFileDialog file_dialog(this);
    file_dialog.setAcceptMode(QFileDialog::AcceptSave);
    file_dialog.setFileMode(QFileDialog::AnyFile);

    QHash<QString, BufferExporter::OutputType> output_extensions;
    output_extensions[tr("Image File (*.png)")] =
        BufferExporter::OutputType::Bitmap;
    output_extensions[tr("Octave Raw Matrix (*.oct)")] =
        BufferExporter::OutputType::OctaveMatrix;

    // Generate the save suffix string
    QHashIterator it(output_extensions);

    QString save_message;

    while (it.hasNext()) {
        it.next();
        save_message += it.key();
        if (it.hasNext()) {
            save_message += ";;";
        }
    }

    file_dialog.setNameFilter(save_message);
    file_dialog.selectNameFilter(default_export_suffix_);

    if (file_dialog.exec() == QDialog::Accepted) {
        const string file_name = file_dialog.selectedFiles()[0].toStdString();
        const auto selected_filter = file_dialog.selectedNameFilter();

        // Export buffer
        BufferExporter::export_buffer(
            component, file_name, output_extensions[selected_filter]);

        // Update default export suffix to the previously used suffix
        default_export_suffix_ = selected_filter;

        // Persist settings
        persist_settings_deferred();
    }
}


void MainWindow::show_context_menu(const QPoint& pos)
{
    if (ui_->imageList->itemAt(pos) != nullptr) {
        // Handle global position
        const QPoint globalPos = ui_->imageList->mapToGlobal(pos);

        // Create menu and insert context actions
        QMenu myMenu(this);

        QAction* exportAction =
            myMenu.addAction("Export buffer", this, SLOT(export_buffer()));

        // Add parameter to action: buffer name
        exportAction->setData(ui_->imageList->itemAt(pos)->data(Qt::UserRole));

        // Show context menu at handling position
        myMenu.exec(globalPos);
    }
}


void MainWindow::toggle_go_to_dialog() const
{
    if (!go_to_widget_->isVisible()) {
        vec4 default_goal(0, 0, 0, 0);

        if (currently_selected_stage_ != nullptr) {
            GameObject* cam_obj =
                currently_selected_stage_->get_game_object("camera");
            const auto* cam =
                cam_obj->get_component<Camera>("camera_component");

            default_goal = cam->get_position();
        }

        go_to_widget_->set_defaults(default_goal.x(), default_goal.y());
    }

    go_to_widget_->toggle_visible();
}


void MainWindow::go_to_pixel(const float x, const float y)
{
    if (link_views_enabled_) {
        for (const auto& [_, stage] : stages_) {
            stage->go_to_pixel(x, y);
        }
    } else if (currently_selected_stage_ != nullptr) {
        currently_selected_stage_->go_to_pixel(x, y);
    }

    update_status_bar();

    request_render_update_ = true;
}

} // namespace oid
