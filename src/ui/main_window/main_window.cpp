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

    ui_components_.ui->setupUi(this);

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
    buffer_data_.held_buffers.clear();
    state_.is_window_ready = false;
}


void MainWindow::showWindow()
{
    ui_components_.update_timer.start(
        static_cast<int>(1000.0 / render_framerate_));
    show();
}


void MainWindow::draw() const
{
    const auto lock = std::unique_lock{ui_mutex_};
    if (const auto stage = buffer_data_.currently_selected_stage.lock()) {
        stage->draw();
    }
}


GLCanvas& MainWindow::gl_canvas() const
{
    return *ui_components_.ui->bufferPreview;
}


QSizeF MainWindow::get_icon_size() const
{
    const auto screen_dpi_scale = get_screen_dpi_scale();
    return {UIConstants::ICON_WIDTH_BASE * screen_dpi_scale,
            UIConstants::ICON_HEIGHT_BASE * screen_dpi_scale};
}


bool MainWindow::is_window_ready() const
{
    const auto lock = std::unique_lock{ui_mutex_};
    return ui_components_.ui->bufferPreview->is_ready() &&
           state_.is_window_ready;
}


void MainWindow::loop()
{
    decode_incoming_messages();

    const auto lock = std::unique_lock{ui_mutex_};
    if (state_.completer_updated) {
        // Update auto-complete suggestion list
        ui_components_.symbol_completer->update_symbol_list(
            buffer_data_.available_vars);
        state_.completer_updated = false;
    }

    // Run update for current stage
    if (const auto stage = buffer_data_.currently_selected_stage.lock()) {
        stage->update();
    }

    // Update visualization pane
    if (state_.request_render_update) {
        ui_components_.ui->bufferPreview->update();
        update_status_bar();
        state_.request_render_update = false;
    }

    // Update an icon of every entry in image list
    if (state_.request_icons_update) {

        for (const auto& name : buffer_data_.stages | std::views::keys) {
            repaint_image_list_icon(name);
        }

        state_.request_icons_update = false;
    }
}


void MainWindow::request_render_update()
{
    const auto lock              = std::unique_lock{ui_mutex_};
    state_.request_render_update = true;
}


void MainWindow::request_icons_update()
{
    const auto lock             = std::unique_lock{ui_mutex_};
    state_.request_icons_update = true;
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

    const auto lock = std::unique_lock{ui_mutex_};
    // Of the buffers not currently being visualized, only keep those whose
    // timer hasn't expired yet and is not in the set of removed names
    for (const auto& prev_buff : previous_session_buffers_qlist) {
        const auto& [buff_name, timestamp] = prev_buff;
        const auto buff_name_std_str       = buff_name.toStdString();

        const auto being_viewed =
            buffer_data_.held_buffers.contains(buff_name_std_str);
        const auto was_removed =
            buffer_data_.removed_buffer_names.contains(buff_name_std_str);

        if (was_removed) {
            buffer_data_.previous_session_buffers.erase(buff_name_std_str);
        } else if (!being_viewed && timestamp >= now) {
            persisted_session_buffers.append(prev_buff);
        }
    }

    for (const auto& buffer : buffer_data_.held_buffers | std::views::keys) {
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
        const auto listSizesInt = ui_components_.ui->splitter->sizes();

        auto listSizesVariant = QList<QVariant>{};
        for (const int size : listSizesInt) {
            listSizesVariant.append(size);
        }

        settings.setValue("splitter", listSizesVariant);
    }
    settings.setValue("minmax_visible", ui_components_.ui->acEdit->isChecked());
    settings.setValue("contrast_enabled",
                      ui_components_.ui->acToggle->isChecked());
    settings.setValue("link_views_enabled",
                      ui_components_.ui->linkViewsToggle->isChecked());
    settings.endGroup();

    // Write window position/size
    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();

    settings.sync();

    buffer_data_.removed_buffer_names.clear();
}


vec4 MainWindow::get_stage_coordinates(const float pos_window_x,
                                       const float pos_window_y) const
{
    const auto stage = buffer_data_.currently_selected_stage.lock();
    if (!stage) {
        return {};
    }
    const auto cam_obj = stage->get_game_object("camera");
    if (!cam_obj.has_value()) {
        return {};
    }
    const auto cam_opt =
        cam_obj->get().get_component<Camera>("camera_component");
    if (!cam_opt.has_value()) {
        return {};
    }
    const auto& cam = cam_opt->get();

    const auto buffer_obj = stage->get_game_object("buffer");
    if (!buffer_obj.has_value()) {
        return {};
    }
    const auto buffer_opt =
        buffer_obj->get().get_component<Buffer>("buffer_component");
    if (!buffer_opt.has_value()) {
        return {};
    }
    const auto& buffer = buffer_opt->get();

    const auto win_w =
        static_cast<float>(ui_components_.ui->bufferPreview->width());
    const auto win_h =
        static_cast<float>(ui_components_.ui->bufferPreview->height());
    const auto mouse_pos_ndc = vec4{2.0f * (pos_window_x - win_w / 2) / win_w,
                                    -2.0f * (pos_window_y - win_h / 2) / win_h,
                                    0.0f,
                                    1.0f};
    const auto view          = cam_obj->get().get_pose().inv();
    const auto buff_pose     = buffer_obj->get().get_pose();
    const auto vp_inv        = (cam.projection * view * buff_pose).inv();

    auto mouse_pos = vp_inv * mouse_pos_ndc;
    mouse_pos += vec4(
        buffer.buffer_width_f / 2.0f, buffer.buffer_height_f / 2.f, 0.0f, 0.0f);

    return mouse_pos;
}


void MainWindow::update_status_bar() const
{
    if (const auto stage = buffer_data_.currently_selected_stage.lock()) {
        auto message = std::stringstream{};

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

        const auto text_comp_opt =
            buffer_obj->get().get_component<BufferValues>("text_component");

        const auto mouse_x =
            static_cast<float>(ui_components_.ui->bufferPreview->mouse_x());
        const auto mouse_y =
            static_cast<float>(ui_components_.ui->bufferPreview->mouse_y());

        // Position
        const auto mouse_pos = get_stage_coordinates(mouse_x, mouse_y);

        // Zoom
        message << std::fixed << std::setprecision(3) << "("
                << static_cast<int>(floor(mouse_pos.x())) << ", "
                << static_cast<int>(floor(mouse_pos.y())) << ")\t"
                << cam.compute_zoom() * 100.0f << "%";

        // Value
        message << " val=";

        buffer.get_pixel_info(message,
                              static_cast<int>(floor(mouse_pos.x())),
                              static_cast<int>(floor(mouse_pos.y())));

        // Float precision
        if ((BufferType::Float64 == buffer.type ||
             BufferType::Float32 == buffer.type) &&
            text_comp_opt.has_value()) {
            const auto& text_comp = text_comp_opt->get();
            message << " precision=[" << text_comp.get_float_precision() << "]";
        }

        ui_components_.status_bar->setText(
            QString::fromStdString(message.str()));
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
    ui_components_.settings_persist_timer.start(100);
}


void MainWindow::set_currently_selected_stage(
    const std::shared_ptr<Stage>& stage)
{
    const auto lock                       = std::unique_lock{ui_mutex_};
    buffer_data_.currently_selected_stage = stage;
    state_.request_render_update          = true;
}

void MainWindow::set_currently_selected_stage(std::nullptr_t)
{
    const auto lock = std::unique_lock{ui_mutex_};
    buffer_data_.currently_selected_stage.reset();
    state_.request_render_update = true;
}

} // namespace oid
