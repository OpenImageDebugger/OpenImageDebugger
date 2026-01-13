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

#include "event_handler.h"

#include <memory>
#include <ranges>

#include <QFileDialog>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMenu>
#include <QPoint>
#include <QString>
#include <QWidget>
#include <QtMath>

#include "io/buffer_exporter.h"
#include "ui/main_window/main_window.h"
#include "visualization/components/buffer.h"
#include "visualization/components/buffer_values.h"
#include "visualization/components/camera.h"
#include "visualization/events.h"
#include "visualization/game_object.h"
#include "visualization/stage.h"


namespace oid
{

UIEventHandler::UIEventHandler(Dependencies deps)
    : deps_{std::move(deps)}
{
}


void UIEventHandler::resize_callback(const int w, const int h) const
{
    const auto lock = std::unique_lock{deps_.ui_mutex};
    for (const auto& stage : deps_.buffer_data.stages | std::views::values) {
        stage->resize_callback(w, h);
    }

    deps_.ui_components.go_to_widget->move(
        deps_.ui_components.ui->bufferPreview->width() -
            deps_.ui_components.go_to_widget->width(),
        deps_.ui_components.ui->bufferPreview->height() -
            deps_.ui_components.go_to_widget->height());
}


void UIEventHandler::scroll_callback(const float delta)
{
    const auto lock = std::unique_lock{deps_.ui_mutex};
    if (deps_.state.link_views_enabled) {
        for (const auto& stage :
             deps_.buffer_data.stages | std::views::values) {
            stage->scroll_callback(delta);
        }
    } else if (const auto stage =
                   deps_.buffer_data.currently_selected_stage.lock()) {
        stage->scroll_callback(delta);
    }

    deps_.update_status_bar();

#if defined(Q_OS_DARWIN)
    deps_.ui_components.ui->bufferPreview->update();
#endif
    deps_.state.request_render_update = true;
}


void UIEventHandler::mouse_drag_event(const int mouse_x, const int mouse_y)
{
    const auto virtual_motion = QPoint{mouse_x, mouse_y};

    const auto lock = std::unique_lock{deps_.ui_mutex};
    if (deps_.state.link_views_enabled) {
        for (const auto& stage :
             deps_.buffer_data.stages | std::views::values) {
            stage->mouse_drag_event(virtual_motion.x(), virtual_motion.y());
        }
    } else if (const auto stage =
                   deps_.buffer_data.currently_selected_stage.lock()) {
        stage->mouse_drag_event(virtual_motion.x(), virtual_motion.y());
    }

    deps_.state.request_render_update = true;
}


void UIEventHandler::mouse_move_event(int, int) const
{
    deps_.update_status_bar();
}


void UIEventHandler::propagate_key_press_event(
    const QKeyEvent* key_event,
    EventProcessCode& event_intercepted) const
{
    const auto lock = std::unique_lock{deps_.ui_mutex};
    for (const auto& stage : deps_.buffer_data.stages | std::views::values) {
        if (stage->key_press_event(key_event->key()) ==
            EventProcessCode::INTERCEPTED) {
            event_intercepted = EventProcessCode::INTERCEPTED;
        }
    }
}


void UIEventHandler::recenter_buffer()
{
    const auto lock                 = std::unique_lock{deps_.ui_mutex};
    const auto recenter_camera_impl = [](Stage& stage) {
        const auto cam_obj = stage.get_game_object("camera");
        if (!cam_obj.has_value()) {
            return;
        }
        const auto cam_opt =
            cam_obj->get().get_component<Camera>("camera_component");
        if (!cam_opt.has_value()) {
            return;
        }
        auto& cam = cam_opt->get();
        cam.recenter_camera();
    };

    if (deps_.state.link_views_enabled) {
        for (const auto& stage :
             deps_.buffer_data.stages | std::views::values) {
            recenter_camera_impl(*stage);
        }
    } else {
        if (const auto stage =
                deps_.buffer_data.currently_selected_stage.lock()) {
            recenter_camera_impl(*stage);
        }
    }

    deps_.state.request_render_update = true;
}


void UIEventHandler::link_views_toggle()
{
    const auto lock                = std::unique_lock{deps_.ui_mutex};
    deps_.state.link_views_enabled = !deps_.state.link_views_enabled;
}


void UIEventHandler::decrease_float_precision()
{
    const auto shift_precision_left_impl = [](Stage& stage) {
        const auto buffer_obj = stage.get_game_object("buffer");
        if (buffer_obj.has_value()) {
            const auto buffer_comp_opt =
                buffer_obj->get().get_component<BufferValues>("text_component");
            if (buffer_comp_opt.has_value()) {
                auto& buffer_comp = buffer_comp_opt->get();
                buffer_comp.decrease_float_precision();
            }
        }
    };

    const auto lock = std::unique_lock{deps_.ui_mutex};
    if (deps_.state.link_views_enabled) {
        for (const auto& stage :
             deps_.buffer_data.stages | std::views::values) {
            shift_precision_left_impl(*stage);
        }
    } else {
        if (const auto stage =
                deps_.buffer_data.currently_selected_stage.lock()) {
            shift_precision_left_impl(*stage);
        }
    }

    deps_.state.request_render_update = true;
}


void UIEventHandler::increase_float_precision()
{
    const auto shift_precision_right_impl = [](Stage& stage) {
        const auto buffer_obj = stage.get_game_object("buffer");
        if (buffer_obj.has_value()) {
            const auto buffer_comp_opt =
                buffer_obj->get().get_component<BufferValues>("text_component");
            if (buffer_comp_opt.has_value()) {
                auto& buffer_comp = buffer_comp_opt->get();
                buffer_comp.increase_float_precision();
            }
        }
    };

    const auto lock = std::unique_lock{deps_.ui_mutex};
    if (deps_.state.link_views_enabled) {
        for (const auto& stage :
             deps_.buffer_data.stages | std::views::values) {
            shift_precision_right_impl(*stage);
        }
    } else {
        if (const auto stage =
                deps_.buffer_data.currently_selected_stage.lock()) {
            shift_precision_right_impl(*stage);
        }
    }

    deps_.state.request_render_update = true;
}


void UIEventHandler::update_shift_precision() const
{
    const auto lock = std::unique_lock{deps_.ui_mutex};

    deps_.ui_components.ui->decrease_float_precision->setEnabled(false);
    deps_.ui_components.ui->increase_float_precision->setEnabled(false);

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

    const auto& buffer = buffer_opt->get();
    if (BufferType::Float32 == buffer.type ||
        BufferType::Float64 == buffer.type) {
        deps_.ui_components.ui->decrease_float_precision->setEnabled(true);
        deps_.ui_components.ui->increase_float_precision->setEnabled(true);
    }
}


void UIEventHandler::rotate_90_cw()
{
    const auto request_90_cw_rotation = [](Stage& stage) {
        const auto buffer_obj = stage.get_game_object("buffer");
        if (buffer_obj.has_value()) {
            const auto buffer_comp_opt =
                buffer_obj->get().get_component<Buffer>("buffer_component");
            if (buffer_comp_opt.has_value()) {
                auto& buffer_comp = buffer_comp_opt->get();
                buffer_comp.rotate(90.0f * static_cast<float>(M_PI) / 180.0f);
            }
        }
    };

    const auto lock = std::unique_lock{deps_.ui_mutex};
    if (deps_.state.link_views_enabled) {
        for (const auto& stage :
             deps_.buffer_data.stages | std::views::values) {
            request_90_cw_rotation(*stage);
        }
    } else {
        if (const auto stage =
                deps_.buffer_data.currently_selected_stage.lock()) {
            request_90_cw_rotation(*stage);
        }
    }

    deps_.state.request_render_update = true;
}


void UIEventHandler::rotate_90_ccw()
{
    const auto request_90_ccw_rotation = [](Stage& stage) {
        const auto buffer_obj = stage.get_game_object("buffer");
        if (buffer_obj.has_value()) {
            const auto buffer_comp_opt =
                buffer_obj->get().get_component<Buffer>("buffer_component");
            if (buffer_comp_opt.has_value()) {
                auto& buffer_comp = buffer_comp_opt->get();
                buffer_comp.rotate(-90.0f * static_cast<float>(M_PI) / 180.0f);
            }
        }
    };

    const auto lock = std::unique_lock{deps_.ui_mutex};
    if (deps_.state.link_views_enabled) {
        for (const auto& stage :
             deps_.buffer_data.stages | std::views::values) {
            request_90_ccw_rotation(*stage);
        }
    } else {
        if (const auto stage =
                deps_.buffer_data.currently_selected_stage.lock()) {
            request_90_ccw_rotation(*stage);
        }
    }

    deps_.state.request_render_update = true;
}


void UIEventHandler::buffer_selected(QListWidgetItem* item)
{
    if (item == nullptr) {
        return;
    }

    const auto lock  = std::unique_lock{deps_.ui_mutex};
    const auto stage = deps_.buffer_data.stages.find(
        item->data(Qt::UserRole).toString().toStdString());
    if (stage == deps_.buffer_data.stages.end()) {
        return;
    }

    deps_.set_currently_selected_stage(stage->second);
    deps_.reset_ac_min_labels();
    deps_.reset_ac_max_labels();
    deps_.update_shift_precision();
    deps_.update_status_bar();
}


void UIEventHandler::remove_selected_buffer()
{
    const auto lock = std::unique_lock{deps_.ui_mutex};
    if (deps_.ui_components.ui->imageList->count() > 0 &&
        !deps_.buffer_data.currently_selected_stage.expired()) {
        auto removed_item = std::unique_ptr<QListWidgetItem>{
            deps_.ui_components.ui->imageList->takeItem(
                deps_.ui_components.ui->imageList->currentRow())};
        const auto buffer_name =
            removed_item->data(Qt::UserRole).toString().toStdString();
        deps_.buffer_data.stages.erase(buffer_name);
        deps_.buffer_data.held_buffers.erase(buffer_name);
        removed_item.reset();

        deps_.buffer_data.removed_buffer_names.insert(buffer_name);

        if (deps_.buffer_data.stages.empty()) {
            deps_.set_currently_selected_stage_null();
            deps_.update_shift_precision();
        }

        deps_.persist_settings_deferred();
    }
}


void UIEventHandler::symbol_selected()
{
    if (deps_.ui_components.ui->symbolList->text().isEmpty()) {
        return;
    }

    const auto symbol_name_qba =
        deps_.ui_components.ui->symbolList->text().toLocal8Bit();
    const auto symbol_name = symbol_name_qba.constData();
    deps_.request_plot_buffer(symbol_name);
    deps_.ui_components.ui->symbolList->setText("");
}


void UIEventHandler::symbol_completed(const QString& str)
{
    if (str.isEmpty()) {
        return;
    }

    const auto symbol_name_qba = str.toLocal8Bit();
    deps_.request_plot_buffer(symbol_name_qba.constData());
    deps_.ui_components.ui->symbolList->setText("");
    deps_.ui_components.ui->symbolList->clearFocus();
}


void UIEventHandler::export_buffer(const QString& buffer_name)
{
    const auto lock = std::unique_lock{deps_.ui_mutex};
    const auto stage =
        deps_.buffer_data.stages.find(buffer_name.toStdString())->second;

    const auto buffer_obj = stage->get_game_object("buffer");
    if (!buffer_obj.has_value()) {
        return;
    }
    const auto component_opt =
        buffer_obj->get().get_component<Buffer>("buffer_component");
    if (!component_opt.has_value()) {
        return;
    }
    const auto& component = component_opt->get();

    auto file_dialog = QFileDialog{deps_.parent_widget};
    file_dialog.setAcceptMode(QFileDialog::AcceptSave);
    file_dialog.setFileMode(QFileDialog::AnyFile);

    auto output_extensions = QHash<QString, BufferExporter::OutputType>{};
    output_extensions[QObject::tr("Image File (*.png)")] =
        BufferExporter::OutputType::Bitmap;
    output_extensions[QObject::tr("Octave Raw Matrix (*.oct)")] =
        BufferExporter::OutputType::OctaveMatrix;

    auto it = QHashIterator{output_extensions};

    auto save_message = QString{};

    while (it.hasNext()) {
        it.next();
        save_message += it.key();
        if (it.hasNext()) {
            save_message += ";;";
        }
    }

    file_dialog.setNameFilter(save_message);
    file_dialog.selectNameFilter(deps_.default_export_suffix);

    if (file_dialog.exec() == QDialog::Accepted) {
        const auto file_name = file_dialog.selectedFiles()[0].toStdString();
        const auto selected_filter = file_dialog.selectedNameFilter();

        BufferExporter::export_buffer(
            component, file_name, output_extensions[selected_filter]);

        deps_.default_export_suffix = selected_filter;

        deps_.persist_settings_deferred();
    }
}


void UIEventHandler::show_context_menu(const QPoint& pos)
{
    if (deps_.ui_components.ui->imageList->itemAt(pos) != nullptr) {
        const auto globalPos =
            deps_.ui_components.ui->imageList->mapToGlobal(pos);

        auto menu = QMenu{deps_.parent_widget};

        const auto buffer_name =
            deps_.ui_components.ui->imageList->itemAt(pos)->data(Qt::UserRole);
        menu.addAction("Export buffer", [this, buffer_name]() {
            export_buffer(buffer_name.toString());
        });

        menu.exec(globalPos);
    }
}


void UIEventHandler::toggle_go_to_dialog() const
{
    const auto lock = std::unique_lock{deps_.ui_mutex};
    if (!deps_.ui_components.go_to_widget->isVisible()) {
        auto default_goal = vec4{0.0f, 0.0f, 0.0f, 0.0f};

        if (const auto stage =
                deps_.buffer_data.currently_selected_stage.lock()) {
            const auto cam_obj = stage->get_game_object("camera");
            if (!cam_obj.has_value()) {
                return;
            }
            const auto cam_opt =
                cam_obj->get().get_component<Camera>("camera_component");
            if (!cam_opt.has_value()) {
                return;
            }
            const auto& cam = cam_opt->get();
            default_goal    = cam.get_position();
        }

        deps_.ui_components.go_to_widget->set_defaults(default_goal.x(),
                                                       default_goal.y());
    }

    deps_.ui_components.go_to_widget->toggle_visible();
}


void UIEventHandler::go_to_pixel(const float x, const float y)
{
    const auto lock = std::unique_lock{deps_.ui_mutex};
    if (deps_.state.link_views_enabled) {
        for (const auto& stage :
             deps_.buffer_data.stages | std::views::values) {
            stage->go_to_pixel(x, y);
        }
    } else if (const auto stage =
                   deps_.buffer_data.currently_selected_stage.lock()) {
        stage->go_to_pixel(x, y);
    }

    deps_.state.request_render_update = true;
}

} // namespace oid
