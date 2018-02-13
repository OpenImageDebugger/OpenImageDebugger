/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 GDB ImageWatch contributors
 * (github.com/csantosbh/gdb-imagewatch/)
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

#include <QFileDialog>

#include "main_window.h"

#include "io/buffer_exporter.h"
#include "ui_main_window.h"
#include "visualization/components/camera.h"
#include "visualization/game_object.h"


using namespace std;


void MainWindow::resize_callback(int w, int h)
{
    for (auto& stage : stages_)
        stage.second->resize_callback(w, h);

    go_to_widget_->move(ui_->bufferPreview->width() - go_to_widget_->width(),
                        ui_->bufferPreview->height() - go_to_widget_->height());
}


void MainWindow::scroll_callback(float delta)
{
    if (link_views_enabled_) {
        for (auto& stage : stages_) {
            stage.second->scroll_callback(delta);
        }
    } else if (currently_selected_stage_ != nullptr) {
        currently_selected_stage_->scroll_callback(delta);
    }

    update_status_bar();

    request_render_update_ = true;
}


void MainWindow::mouse_drag_event(int mouse_x, int mouse_y)
{
    const qreal screen_dpi_scale = get_screen_dpi_scale();
    const QPoint virtual_motion(mouse_x * screen_dpi_scale,
                                mouse_y * screen_dpi_scale);

    if (link_views_enabled_) {
        for (auto& stage : stages_)
            stage.second->mouse_drag_event(virtual_motion.x(),
                                           virtual_motion.y());
    } else if (currently_selected_stage_ != nullptr) {
        currently_selected_stage_->mouse_drag_event(virtual_motion.x(),
                                                    virtual_motion.y());
    }

    request_render_update_ = true;
}


void MainWindow::mouse_move_event(int, int)
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
        QKeyEvent* key_event = static_cast<QKeyEvent*>(event);

        EventProcessCode event_intercepted = EventProcessCode::IGNORED;

        if (link_views_enabled_) {
            for (auto& stage : stages_) {
                EventProcessCode event_intercepted_stage =
                    stage.second->key_press_event(key_event->key());

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
        } else {
            return QObject::eventFilter(target, event);
        }
    }

    return false;
}


void MainWindow::recenter_buffer()
{
    if (link_views_enabled_) {
        for (auto& stage : stages_) {
            GameObject* cam_obj = stage.second->get_game_object("camera");
            Camera* cam = cam_obj->get_component<Camera>("camera_component");
            cam->recenter_camera();
        }
    } else {
        if (currently_selected_stage_ != nullptr) {
            GameObject* cam_obj =
                currently_selected_stage_->get_game_object("camera");
            Camera* cam = cam_obj->get_component<Camera>("camera_component");
            cam->recenter_camera();
        }
    }

    request_render_update_ = true;
}


void MainWindow::link_views_toggle()
{
    link_views_enabled_ = !link_views_enabled_;
}


void MainWindow::rotate_90_cw()
{
    const auto request_90_cw_rotation = [](Stage* stage) {
        GameObject* buffer_obj = stage->get_game_object("buffer");
        Buffer* buffer_comp =
            buffer_obj->get_component<Buffer>("buffer_component");

        buffer_comp->rotate(90.f * M_PI / 180.f);
    };

    if (link_views_enabled_) {
        for (auto& stage : stages_) {
            request_90_cw_rotation(stage.second.get());
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
        Buffer* buffer_comp =
            buffer_obj->get_component<Buffer>("buffer_component");

        buffer_comp->rotate(-90.f * M_PI / 180.f);
    };

    if (link_views_enabled_) {
        for (auto& stage : stages_) {
            request_90_ccw_rotation(stage.second.get());
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
    if (item == nullptr)
        return;

    auto stage =
        stages_.find(item->data(Qt::UserRole).toString().toStdString());
    if (stage != stages_.end()) {
        set_currently_selected_stage(stage->second.get());
        reset_ac_min_labels();
        reset_ac_max_labels();

        update_status_bar();
    }
}


void MainWindow::remove_selected_buffer()
{
    if (ui_->imageList->count() > 0 && currently_selected_stage_ != nullptr) {
        QListWidgetItem* removed_item =
            ui_->imageList->takeItem(ui_->imageList->currentRow());
        string buffer_name =
            removed_item->data(Qt::UserRole).toString().toStdString();
        stages_.erase(buffer_name);
        held_buffers_.erase(buffer_name);
        delete removed_item;

        removed_buffer_names_.insert(buffer_name);

        if (stages_.size() == 0) {
            set_currently_selected_stage(nullptr);
        }

        persist_settings_deferred();
    }
}


void MainWindow::symbol_selected()
{
    QByteArray symbol_name_qba = ui_->symbolList->text().toLocal8Bit();
    const char* symbol_name    = symbol_name_qba.constData();
    if (ui_->symbolList->text().length() > 0) {
        plot_callback_(symbol_name);
        // Clear symbol input
        ui_->symbolList->setText("");
    }
}


void MainWindow::symbol_completed(QString str)
{
    if (str.length() > 0) {
        QByteArray symbol_name_qba = str.toLocal8Bit();
        plot_callback_(symbol_name_qba.constData());
        // Clear symbol input
        ui_->symbolList->setText("");
        ui_->symbolList->clearFocus();
    }
}


void MainWindow::export_buffer()
{
    auto sender_action(static_cast<QAction*>(sender()));

    auto stage =
        stages_.find(sender_action->data().toString().toStdString())->second;

    GameObject* buffer_obj = stage->get_game_object("buffer");
    Buffer* component = buffer_obj->get_component<Buffer>("buffer_component");

    QFileDialog file_dialog(this);
    file_dialog.setAcceptMode(QFileDialog::AcceptSave);
    file_dialog.setFileMode(QFileDialog::AnyFile);

    QHash<QString, BufferExporter::OutputType> output_extensions;
    output_extensions[tr("Image File (*.png)")] =
        BufferExporter::OutputType::Bitmap;
    output_extensions[tr("Octave Raw Matrix (*.oct)")] =
        BufferExporter::OutputType::OctaveMatrix;

    // Generate the save suffix string
    QHashIterator<QString, BufferExporter::OutputType> it(output_extensions);

    QString save_message;

    while (it.hasNext()) {
        it.next();
        save_message += it.key();
        if (it.hasNext())
            save_message += ";;";
    }

    file_dialog.setNameFilter(save_message);
    file_dialog.selectNameFilter(default_export_suffix_);

    if (file_dialog.exec() == QDialog::Accepted) {
        string file_name = file_dialog.selectedFiles()[0].toStdString();
        const auto selected_filter = file_dialog.selectedNameFilter();

        // Export buffer
        BufferExporter::export_buffer(
            component,
            file_name,
            output_extensions[selected_filter]);

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
        QPoint globalPos = ui_->imageList->mapToGlobal(pos);

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


void MainWindow::toggle_go_to_dialog()
{
    if (!go_to_widget_->isVisible()) {
        vec4 default_goal(0, 0, 0, 0);

        if (currently_selected_stage_ != nullptr) {
            GameObject* cam_obj =
                currently_selected_stage_->get_game_object("camera");
            Camera* cam = cam_obj->get_component<Camera>("camera_component");

            default_goal = cam->get_position();
        }

        go_to_widget_->set_defaults(default_goal.x(), default_goal.y());
    }

    go_to_widget_->toggle_visible();
}


void MainWindow::go_to_pixel(float x, float y)
{
    if (link_views_enabled_) {
        for (auto& stage : stages_) {
            stage.second->go_to_pixel(x, y);
        }
    } else if (currently_selected_stage_ != nullptr) {
        currently_selected_stage_->go_to_pixel(x, y);
    }

    update_status_bar();

    request_render_update_ = true;
}
