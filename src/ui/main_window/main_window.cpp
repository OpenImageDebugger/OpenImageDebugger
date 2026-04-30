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

#include <chrono>
#include <fstream>
#include <iomanip>
#include <ranges>
#include <utility>
#include <cstdio>
#include <cerrno>

#include <QDateTime>
#include <QScreen>
#include <QSettings>

#include "main_window_initializer.h"
#include "math/linear_algebra.h"
#include "settings_applier.h"
#include "settings_manager.h"
#include "ui_main_window.h"
#include "visualization/components/buffer_values.h"
#include "visualization/components/camera.h"
#include "visualization/game_object.h"
#include "visualization/stage.h"


Q_DECLARE_METATYPE(QList<QString>) // namespace oid

namespace oid
{

namespace
{

enum class PixelDisplayFormat { BGRA, RGBA, Channel0, Channel1, Channel2 };

// Convert QString to PixelDisplayFormat enum
PixelDisplayFormat string_to_format(const QString& format_str)
{
    const auto format_lower = format_str.toLower().toStdString();
    if (format_lower == "bgra") {
        return PixelDisplayFormat::BGRA;
    }
    if (format_lower == "rgba") {
        return PixelDisplayFormat::RGBA;
    }
    if (format_lower == "channel0") {
        return PixelDisplayFormat::Channel0;
    }
    if (format_lower == "channel1") {
        return PixelDisplayFormat::Channel1;
    }
    if (format_lower == "channel2") {
        return PixelDisplayFormat::Channel2;
    }
    // Default fallback
    return PixelDisplayFormat::BGRA;
}

// Apply pixel display format to buffer
void apply_format(Buffer& buffer, PixelDisplayFormat format)
{
    switch (format) {
    case PixelDisplayFormat::Channel0:
        if (buffer.channels > 0) {
            buffer.set_display_channel_mode(1);
            buffer.set_pixel_layout("rrra");
        }
        break;
    case PixelDisplayFormat::Channel1:
        if (buffer.channels > 1) {
            buffer.set_display_channel_mode(1);
            buffer.set_pixel_layout("ggga");
        }
        break;
    case PixelDisplayFormat::Channel2:
        if (buffer.channels > 2) {
            buffer.set_display_channel_mode(1);
            buffer.set_pixel_layout("bbba");
        }
        break;
    case PixelDisplayFormat::BGRA:
        buffer.set_display_channel_mode(-1);
        buffer.set_pixel_layout("bgra");
        break;
    case PixelDisplayFormat::RGBA:
        buffer.set_display_channel_mode(-1);
        buffer.set_pixel_layout("rgba");
        break;
    }
}

} // namespace

MainWindow::MainWindow(ConnectionSettings host_settings, QWidget* parent)
    : QMainWindow{parent}
    , host_settings_{std::move(host_settings)}
{
    QCoreApplication::instance()->installEventFilter(this);
    ui_components_.ui->setupUi(this);

    // Must be done after setupUi() so the widget exists
    gl_canvas_ptr_ =
        std::shared_ptr<GLCanvas>(ui_components_.ui->bufferPreview,
                                  [](GLCanvas*) { /* no-op: Qt owns it */ });

    // Initialize auto-contrast controller
    ac_controller_ = std::make_unique<AutoContrastController>(
        AutoContrastController::Dependencies{
            ui_mutex_, buffer_data_, state_, ui_components_});

    // Initialize message handler
    message_handler_ = std::make_unique<MessageHandler>(
        MessageHandler::Dependencies{
            ui_mutex_,
            buffer_data_,
            state_,
            ui_components_,
            socket_,
            [this]() { return get_icon_size(); },
            [this]() { return std::make_shared<Stage>(*this); }},
        this);

    // Initialize UI event handler
    event_handler_ = std::make_unique<UIEventHandler>(
        UIEventHandler::Dependencies{ui_mutex_,
                                     buffer_data_,
                                     state_,
                                     ui_components_,
                                     channel_names_,
                                     default_export_suffix_,
                                     gl_canvas_ptr_},
        this);

    // Initialize settings manager
    settings_manager_ = std::make_unique<SettingsManager>(this);

    // Initialize settings applier
    settings_applier_ = std::make_unique<SettingsApplier>(
        SettingsApplier::Dependencies{ui_mutex_,
                                      state_,
                                      ui_components_,
                                      buffer_data_,
                                      channel_names_,
                                      render_framerate_,
                                      default_export_suffix_,
                                      *this},
        this);

    connect_settings_signals();

    // Initialize main window initializer
    initializer_ = std::make_unique<MainWindowInitializer>(
        MainWindowInitializer::Dependencies{state_,
                                            ui_components_,
                                            buffer_data_,
                                            channel_names_,
                                            host_settings_,
                                            socket_,
                                            *this,
                                            *ac_controller_,
                                            *event_handler_,
                                            *settings_manager_});

    // Connect UIEventHandler signals to MainWindow slots
    connect(event_handler_.get(),
            &UIEventHandler::statusBarUpdateRequested,
            this,
            &MainWindow::update_status_bar);
    connect(event_handler_.get(),
            &UIEventHandler::stageSelectionRequested,
            this,
            [this](const std::shared_ptr<Stage>& stage) {
                set_currently_selected_stage(stage);
            });
    connect(event_handler_.get(),
            &UIEventHandler::stageSelectionCleared,
            this,
            [this]() { set_currently_selected_stage(nullptr); });
    connect(event_handler_.get(),
            &UIEventHandler::plotBufferRequested,
            this,
            [this](const QString& buffer_name) {
                message_handler_->request_plot_buffer(
                    std::string_view(buffer_name.toLocal8Bit().constData()));
            });
    connect(event_handler_.get(),
            &UIEventHandler::acMinLabelsResetRequested,
            this,
            [this]() { ac_controller_->reset_min_labels(); });
    connect(event_handler_.get(),
            &UIEventHandler::acMaxLabelsResetRequested,
            this,
            [this]() { ac_controller_->reset_max_labels(); });
    connect(event_handler_.get(),
            &UIEventHandler::shiftPrecisionUpdateRequested,
            this,
            [this]() { event_handler_->update_shift_precision(); });
    connect(event_handler_.get(),
            &UIEventHandler::settingsPersistenceRequested,
            this,
            &MainWindow::persist_settings_deferred);

    // Connect MessageHandler signals to MainWindow slots
    connect(message_handler_.get(),
            &MessageHandler::acMinLabelsResetRequested,
            this,
            [this]() { ac_controller_->reset_min_labels(); });
    connect(message_handler_.get(),
            &MessageHandler::acMaxLabelsResetRequested,
            this,
            [this]() { ac_controller_->reset_max_labels(); });
    connect(message_handler_.get(),
            &MessageHandler::settingsPersistenceRequested,
            this,
            &MainWindow::persist_settings_deferred);

    // Load settings (must be done before initialization)
    settings_manager_->load_settings();

    // Initialize UI components
    initializer_->initialize();
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


std::shared_ptr<GLCanvas> MainWindow::gl_canvas() const
{
    return gl_canvas_ptr_;
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
    message_handler_->decode_incoming_messages();

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
            message_handler_->repaint_image_list_icon(name);
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
    SettingsManager::DataCallbacks callbacks;

    // Window geometry
    callbacks.getWindowSize = [this]() { return size(); };
    callbacks.getWindowPos  = [this]() { return pos(); };

    // UI state
    callbacks.getSplitterSizes = [this]() {
        return ui_components_.ui->splitter->sizes();
    };
    callbacks.getMinMaxVisible = [this]() {
        return ui_components_.ui->acEdit->isChecked();
    };
    callbacks.getContrastEnabled = [this]() {
        return ui_components_.ui->acToggle->isChecked();
    };
    callbacks.getLinkViewsEnabled = [this]() {
        return ui_components_.ui->linkViewsToggle->isChecked();
    };

    // Application state
    callbacks.getRenderFramerate     = [this]() { return render_framerate_; };
    callbacks.getDefaultExportSuffix = [this]() {
        return default_export_suffix_;
    };

    // Buffer data (with mutex protection)
    callbacks.getCurrentBufferNames = [this]() {
        const auto lock = std::unique_lock{ui_mutex_};
        std::set<std::string, std::less<>> names;
        for (const auto& buffer :
             buffer_data_.held_buffers | std::views::keys) {
            names.insert(buffer);
        }
        return names;
    };
    callbacks.getPreviousSessionBuffers = [this]() {
        const auto lock = std::unique_lock{ui_mutex_};
        std::set<std::string, std::less<>> result;
        for (const auto& name : buffer_data_.previous_session_buffers) {
            result.insert(name);
        }
        return result;
    };
    callbacks.getRemovedBufferNames = [this]() {
        const auto lock = std::unique_lock{ui_mutex_};
        std::set<std::string, std::less<>> result;
        for (const auto& name : buffer_data_.removed_buffer_names) {
            result.insert(name);
        }
        return result;
    };
    callbacks.clearRemovedBufferNames = [this]() {
        const auto lock = std::unique_lock{ui_mutex_};
        buffer_data_.removed_buffer_names.clear();
    };

    settings_manager_->persist_settings(callbacks);
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
    using namespace oid::SettingsConstants;
    ui_components_.settings_persist_timer.start(SETTINGS_PERSIST_DELAY_MS);
}


void MainWindow::pixel_format_changed(const QString& format)
{
    const auto lock = std::unique_lock{ui_mutex_};

    const auto stage = buffer_data_.currently_selected_stage.lock();
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

    auto& buffer = buffer_opt->get();

    // Convert string to enum and apply format
    const auto format_enum = string_to_format(format);
    apply_format(buffer, format_enum);

    state_.request_render_update = true;
}


void MainWindow::set_currently_selected_stage(
    const std::shared_ptr<Stage>& stage)
{
    const auto lock                       = std::unique_lock{ui_mutex_};
    buffer_data_.currently_selected_stage = stage;

    // Enable pixel format dropdown only if channels >= 3
    bool enable_dropdown = false;
    if (stage) {
        const auto buffer_obj = stage->get_game_object("buffer");
        if (buffer_obj.has_value()) {
            const auto buffer_opt =
                buffer_obj->get().get_component<Buffer>("buffer_component");
            if (buffer_opt.has_value()) {
                enable_dropdown = buffer_opt->get().channels >= 3;
            }
        }
    }
    ui_components_.ui->pixelFormatSelector->setEnabled(enable_dropdown);

    state_.request_render_update = true;
}

void MainWindow::set_currently_selected_stage(std::nullptr_t)
{
    const auto lock = std::unique_lock{ui_mutex_};
    buffer_data_.currently_selected_stage.reset();
    ui_components_.ui->pixelFormatSelector->setEnabled(false);
    state_.request_render_update = true;
}

void MainWindow::connect_settings_signals() const
{
    const auto* manager = settings_manager_.get();
    const auto* applier = settings_applier_.get();

    connect(manager,
            &SettingsManager::renderingSettingsLoaded,
            applier,
            &SettingsApplier::apply_rendering_settings);
    connect(manager,
            &SettingsManager::exportSettingsLoaded,
            applier,
            &SettingsApplier::apply_export_settings);
    connect(manager,
            &SettingsManager::windowGeometryLoaded,
            applier,
            &SettingsApplier::apply_window_geometry);
    connect(manager,
            &SettingsManager::windowResizeRestoreRequested,
            applier,
            &SettingsApplier::restore_window_resize);
    connect(manager,
            &SettingsManager::uiListPositionLoaded,
            applier,
            &SettingsApplier::apply_ui_list_position);
    connect(manager,
            &SettingsManager::uiSplitterSizesLoaded,
            applier,
            &SettingsApplier::apply_ui_splitter_sizes);
    connect(manager,
            &SettingsManager::uiMinMaxCompactLoaded,
            applier,
            &SettingsApplier::apply_ui_minmax_compact);
    connect(manager,
            &SettingsManager::uiColorspaceLoaded,
            applier,
            &SettingsApplier::apply_ui_colorspace);
    connect(manager,
            &SettingsManager::uiMinMaxVisibleLoaded,
            applier,
            &SettingsApplier::apply_ui_minmax_visible);
    connect(manager,
            &SettingsManager::uiContrastEnabledLoaded,
            applier,
            &SettingsApplier::apply_ui_contrast_enabled);
    connect(manager,
            &SettingsManager::uiLinkViewsEnabledLoaded,
            applier,
            &SettingsApplier::apply_ui_link_views_enabled);
    connect(manager,
            &SettingsManager::previousSessionBuffersLoaded,
            applier,
            &SettingsApplier::apply_previous_session_buffers);
}

} // namespace oid
