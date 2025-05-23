/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2025 OpenImageDebugger contributors
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

#include <iomanip>
#include <ranges>
#include <utility>

#include <QDateTime>
#include <QScreen>
#include <QSettings>

#include "math/linear_algebra.h"
#include "ui_main_window.h"
#include "visualization/components/buffer_values.h"
#include "visualization/components/camera.h"
#include "visualization/game_object.h"


Q_DECLARE_METATYPE(QList<QString>)

namespace oid
{

MainWindow::MainWindow(ConnectionSettings host_settings, QWidget* parent)
    : QMainWindow{parent}
    , host_settings_{std::move(host_settings)}
{
    QCoreApplication::instance()->installEventFilter(this);

    ui_->setupUi(this);

    initialize_settings();
    initialize_ui_icons();
    initialize_ui_signals();
    initialize_timers();
    initialize_symbol_completer();
    initialize_left_pane();
    initialize_auto_contrast_form();
    initialize_toolbar();
    initialize_status_bar();
    initialize_visualization_pane();
    initialize_go_to_widget();
    initialize_shortcuts();
    initialize_networking();
}


MainWindow::~MainWindow()
{
    held_buffers_.clear();
    is_window_ready_ = false;
}


void MainWindow::showWindow()
{
    update_timer_.start(static_cast<int>(1000.0 / render_framerate_));
    show();
}


void MainWindow::draw() const
{
    if (currently_selected_stage_ != nullptr) {
        currently_selected_stage_->draw();
    }
}


GLCanvas* MainWindow::gl_canvas() const
{
    return ui_->bufferPreview;
}


QSizeF MainWindow::get_icon_size() const
{
    const auto screen_dpi_scale = get_screen_dpi_scale();
    return {icon_width_base_ * screen_dpi_scale,
            icon_height_base_ * screen_dpi_scale};
}


bool MainWindow::is_window_ready() const
{
    return ui_->bufferPreview->is_ready() && is_window_ready_;
}


void MainWindow::loop()
{
    decode_incoming_messages();

    if (completer_updated_) {
        // Update auto-complete suggestion list
        symbol_completer_->update_symbol_list(available_vars_);
        completer_updated_ = false;
    }

    // Run update for current stage
    if (currently_selected_stage_ != nullptr) {
        currently_selected_stage_->update();
    }

    // Update visualization pane
    if (request_render_update_) {
        ui_->bufferPreview->update();
        update_status_bar();
        request_render_update_ = false;
    }

    // Update an icon of every entry in image list
    if (request_icons_update_) {

        for (const auto& name : stages_ | std::views::keys) {
            repaint_image_list_icon(name);
        }

        request_icons_update_ = false;
    }
}


void MainWindow::request_render_update()
{
    request_render_update_ = true;
}


void MainWindow::request_icons_update()
{
    request_icons_update_ = true;
}


void MainWindow::persist_settings()
{
    using BufferExpiration = QPair<QString, QDateTime>;

    auto settings = QSettings{QSettings::Format::IniFormat,
                              QSettings::Scope::UserScope,
                              "OpenImageDebugger"};

    auto persisted_session_buffers = QList<BufferExpiration>{};

    // Load previous session symbols
    const auto previous_session_buffers_qlist =
        settings.value("PreviousSession/buffers")
            .value<QList<BufferExpiration>>();

    const auto now             = QDateTime::currentDateTime();
    const auto next_expiration = now.addDays(1);

    // Of the buffers not currently being visualized, only keep those whose
    // timer hasn't expired yet and is not in the set of removed names
    for (const auto& prev_buff : previous_session_buffers_qlist) {
        const auto buff_name_std_str = prev_buff.first.toStdString();

        const auto being_viewed = held_buffers_.contains(buff_name_std_str);
        const auto was_removed =
            removed_buffer_names_.contains(buff_name_std_str);

        if (was_removed) {
            previous_session_buffers_.erase(buff_name_std_str);
        } else if (!being_viewed && prev_buff.second >= now) {
            persisted_session_buffers.append(prev_buff);
        }
    }

    for (const auto& buffer : held_buffers_ | std::views::keys) {
        persisted_session_buffers.append(
            BufferExpiration(buffer.c_str(), next_expiration));
    }

    // Write default suffix for buffer export
    settings.setValue("Export/default_export_suffix", default_export_suffix_);

    // Write maximum framerate
    settings.setValue("Rendering/maximum_framerate", render_framerate_);

    // Write previous session symbols
    settings.setValue("PreviousSession/buffers",
                      QVariant::fromValue(persisted_session_buffers));

    // Write UI geometry.
    settings.beginGroup("UI");
    {
        const auto listSizesInt = ui_->splitter->sizes();

        auto listSizesVariant = QList<QVariant>{};
        for (const int size : listSizesInt) {
            listSizesVariant.append(size);
        }

        settings.setValue("splitter", listSizesVariant);
    }
    settings.setValue("minmax_visible", ui_->acEdit->isChecked());
    settings.setValue("contrast_enabled", ui_->acToggle->isChecked());
    settings.setValue("link_views_enabled", ui_->linkViewsToggle->isChecked());
    settings.endGroup();

    // Write window position/size
    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();

    settings.sync();

    removed_buffer_names_.clear();
}


vec4 MainWindow::get_stage_coordinates(const float pos_window_x,
                                       const float pos_window_y) const
{
    const auto cam_obj = currently_selected_stage_->get_game_object("camera");
    const auto cam     = cam_obj->get_component<Camera>("camera_component");

    const auto buffer_obj =
        currently_selected_stage_->get_game_object("buffer");
    const auto buffer = buffer_obj->get_component<Buffer>("buffer_component");

    const auto win_w         = static_cast<float>(ui_->bufferPreview->width());
    const auto win_h         = static_cast<float>(ui_->bufferPreview->height());
    const auto mouse_pos_ndc = vec4{2.0f * (pos_window_x - win_w / 2) / win_w,
                                    -2.0f * (pos_window_y - win_h / 2) / win_h,
                                    0.0f,
                                    1.0f};
    const auto view          = cam_obj->get_pose().inv();
    const auto buff_pose     = buffer_obj->get_pose();
    const auto vp_inv        = (cam->projection * view * buff_pose).inv();

    auto mouse_pos = vp_inv * mouse_pos_ndc;
    mouse_pos += vec4(buffer->buffer_width_f / 2.0f,
                      buffer->buffer_height_f / 2.f,
                      0.0f,
                      0.0f);

    return mouse_pos;
}


void MainWindow::update_status_bar() const
{
    if (currently_selected_stage_ != nullptr) {
        auto message = std::stringstream{};

        const auto cam_obj =
            currently_selected_stage_->get_game_object("camera");
        const auto cam = cam_obj->get_component<Camera>("camera_component");

        const auto buffer_obj =
            currently_selected_stage_->get_game_object("buffer");
        const auto buffer =
            buffer_obj->get_component<Buffer>("buffer_component");

        const auto text_comp =
            buffer_obj->get_component<BufferValues>("text_component");

        const auto mouse_x = static_cast<float>(ui_->bufferPreview->mouse_x());
        const auto mouse_y = static_cast<float>(ui_->bufferPreview->mouse_y());

        // Position
        const auto mouse_pos = get_stage_coordinates(mouse_x, mouse_y);

        // Zoom
        message << std::fixed << std::setprecision(3) << "("
                << static_cast<int>(floor(mouse_pos.x())) << ", "
                << static_cast<int>(floor(mouse_pos.y())) << ")\t"
                << cam->compute_zoom() * 100.0f << "%";

        // Value
        message << " val=";

        buffer->get_pixel_info(message,
                               static_cast<int>(floor(mouse_pos.x())),
                               static_cast<int>(floor(mouse_pos.y())));

        // Float precision
        if (BufferType::Float64 == buffer->type ||
            BufferType::Float32 == buffer->type) {
            message << " precision=[" << text_comp->get_float_precision()
                    << "]";
        }

        status_bar_->setText(message.str().c_str());
    }
}


qreal MainWindow::get_screen_dpi_scale()
{
    return QGuiApplication::primaryScreen()->devicePixelRatio();
}


std::string MainWindow::get_type_label(const BufferType type,
                                       const int channels)
{
    auto result = std::stringstream{};
    if (type == BufferType::Float32) {
        result << "float32";
    } else if (type == BufferType::UnsignedByte) {
        result << "uint8";
    } else if (type == BufferType::Short) {
        result << "int16";
    } else if (type == BufferType::UnsignedShort) {
        result << "uint16";
    } else if (type == BufferType::Int32) {
        result << "int32";
    } else if (type == BufferType::Float64) {
        result << "float64";
    }
    result << "x" << channels;

    return result.str();
}


void MainWindow::persist_settings_deferred()
{
    settings_persist_timer_.start(100);
}


void MainWindow::set_currently_selected_stage(Stage* stage)
{
    currently_selected_stage_ = stage;
    request_render_update_    = true;
}

} // namespace oid
