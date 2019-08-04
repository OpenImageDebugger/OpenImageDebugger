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

#include <iomanip>
#include <unistd.h> // TODO remove

#include <QAction>
#include <QDateTime>
#include <QScreen>
#include <QSettings>

#include "main_window.h"

#include "ui_main_window.h"
#include "visualization/components/camera.h"
#include "visualization/game_object.h"
#include "ipc/message_exchange.h"


using namespace std;


Q_DECLARE_METATYPE(QList<QString>)


MainWindow::MainWindow(const ConnectionSettings& host_settings,
                       QWidget* parent)
    : QMainWindow(parent)
    , is_window_ready_(false)
    , request_render_update_(true)
    , completer_updated_(false)
    , ac_enabled_(true)
    , link_views_enabled_(false)
    , icon_width_base_(100)
    , icon_height_base_(50)
    , currently_selected_stage_(nullptr)
    , ui_(new Ui::MainWindowUi)
    , host_settings_(host_settings)
{
    QCoreApplication::instance()->installEventFilter(this);

    ui_->setupUi(this);

    initialize_ui_icons();
    initialize_timers();
    initialize_symbol_completer();
    initialize_left_pane();
    initialize_auto_contrast_form();
    initialize_toolbar();
    initialize_status_bar();
    initialize_visualization_pane();
    initialize_settings();
    initialize_go_to_widget();
    initialize_shortcuts();
    initialize_networking();

    is_window_ready_ = true;
}


MainWindow::~MainWindow()
{
    held_buffers_.clear();
    is_window_ready_ = false;

    delete ui_;
}


void MainWindow::show()
{
    update_timer_.start(static_cast<int>(1000.0 / render_framerate_));
    QMainWindow::show();
}


void MainWindow::draw()
{
    if (currently_selected_stage_ != nullptr) {
        currently_selected_stage_->draw();
    }
}


GLCanvas* MainWindow::gl_canvas()
{
    return ui_->bufferPreview;
}


QSizeF MainWindow::get_icon_size()
{
    const qreal screen_dpi_scale = get_screen_dpi_scale();
    return QSizeF(icon_width_base_ * screen_dpi_scale,
                  icon_height_base_ * screen_dpi_scale);
}


bool MainWindow::is_window_ready()
{
    return ui_->bufferPreview->is_ready() && is_window_ready_;
}


void MainWindow::decode_incoming_messages() {
    available_vars_.clear();

    const auto decode_set_available_symbols = [&]() {
        std::unique_lock<std::mutex> lock(ui_mutex_);
        MessageDecoder message_decoder(&socket_);
        message_decoder.read<QStringList, QString>(available_vars_);

        for(const auto& symbol_value: available_vars_) {
            // Plot buffer if it was available in the previous session
            if (previous_session_buffers_.find(symbol_value.toStdString()) !=
                previous_session_buffers_.end()) {
                request_plot_buffer(symbol_value.toStdString().data());
            }
        }

        completer_updated_ = true;
    };

    const auto respond_get_observed_symbols = [&]() {
        MessageComposer message_composer;
        message_composer.push(MessageType::GetObservedSymbolsResponse)
            .push(held_buffers_.size());
        for (const auto& name : held_buffers_) {
            message_composer.push(name.first);
        }
        message_composer.send(&socket_);
    };

    if(socket_.bytesAvailable() == 0) {
        return;
    }

    MessageType header;
    if(!socket_.read(reinterpret_cast<char*>(&header),
                     static_cast<qint64>(sizeof(header)))) {
        return;
    }

    socket_.waitForReadyRead(100);

    switch(header) {
    case MessageType::SetAvailableSymbols:
        decode_set_available_symbols();
        break;
    case MessageType::GetObservedSymbols:
        respond_get_observed_symbols();
        break;
    case MessageType::PlotBufferContents:
        decode_plot_buffer_contents();
        break;
    default:
        break;
    }
}


void MainWindow::decode_plot_buffer_contents()
{
    // Buffer icon dimensions
    QSizeF icon_size         = get_icon_size();
    int icon_width           = static_cast<int>(icon_size.width());
    int icon_height          = static_cast<int>(icon_size.height());
    const int bytes_per_line = icon_width * 3;

    // Read buffer info
    string variable_name_str;
    string display_name_str;
    string pixel_layout_str;
    bool transpose_buffer;
    int buff_width;
    int buff_height;
    int buff_channels;
    int buff_stride;
    BufferType buff_type;
    vector<uint8_t> buff_contents;

    MessageDecoder message_decoder(&socket_);
    message_decoder.read(variable_name_str)
        .read(display_name_str)
        .read(pixel_layout_str)
        .read(transpose_buffer)
        .read(buff_width)
        .read(buff_height)
        .read(buff_channels)
        .read(buff_stride)
        .read(buff_type)
        .read(buff_contents);

    auto buffer_stage = stages_.find(variable_name_str);

    if(buff_type == BufferType::Float64) {
        held_buffers_[variable_name_str] = make_float_buffer_from_double(buff_contents);
    } else {
        held_buffers_[variable_name_str] = std::move(buff_contents);
    }

    // Human readable dimensions
    int visualized_width;
    int visualized_height;
    if (!transpose_buffer) {
        visualized_width = buff_width;
        visualized_height = buff_height;
    } else {
        visualized_width = buff_height;
        visualized_height = buff_width;
    }

    if (buffer_stage == stages_.end()) { // New buffer request
        shared_ptr<Stage> stage = make_shared<Stage>(this);
        if (!stage->initialize(held_buffers_[variable_name_str].data(),
                               buff_width,
                               buff_height,
                               buff_channels,
                               buff_type,
                               buff_stride,
                               pixel_layout_str,
                               transpose_buffer)) {
            cerr << "[error] Could not initialize opengl canvas!" << endl;
        }
        stage->contrast_enabled            = ac_enabled_;
        stages_[variable_name_str] = stage;

        ui_->bufferPreview->render_buffer_icon(
            stage.get(), icon_width, icon_height);

        QImage bufferIcon(stage->buffer_icon.data(),
                          icon_width,
                          icon_height,
                          bytes_per_line,
                          QImage::Format_RGB888);

        stringstream label;
        label << display_name_str << "\n["
              << visualized_width << "x"
              << visualized_height << "]\n"
              << get_type_label(buff_type, buff_channels);
        QListWidgetItem* item =
            new QListWidgetItem(QPixmap::fromImage(bufferIcon),
                                label.str().c_str(),
                                ui_->imageList);
        item->setData(Qt::UserRole,
                      QString(variable_name_str.c_str()));
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                       Qt::ItemIsDragEnabled);
        ui_->imageList->addItem(item);

        persist_settings_deferred();
    } else { // Update buffer request
        buffer_stage->second->buffer_update(
                    held_buffers_[variable_name_str].data(),
                    buff_width,
                    buff_height,
                    buff_channels,
                    buff_type,
                    buff_stride,
                    pixel_layout_str,
                    transpose_buffer);

        // Update buffer icon
        shared_ptr<Stage>& stage = stages_[variable_name_str];
        ui_->bufferPreview->render_buffer_icon(
            stage.get(), icon_width, icon_height);

        // Looking for corresponding item...
        QImage bufferIcon(stage->buffer_icon.data(),
                          icon_width,
                          icon_height,
                          bytes_per_line,
                          QImage::Format_RGB888);
        stringstream label;
        label << display_name_str << "\n["
              << visualized_width << "x"
              << visualized_height << "]\n"
              << get_type_label(buff_type, buff_channels);

        for (int i = 0; i < ui_->imageList->count(); ++i) {
            QListWidgetItem* item = ui_->imageList->item(i);
            if (item->data(Qt::UserRole) ==
                variable_name_str.c_str()) {
                item->setIcon(QPixmap::fromImage(bufferIcon));
                item->setText(label.str().c_str());
                break;
            }
        }

        // Update AC values
        if (currently_selected_stage_ != nullptr) {
            reset_ac_min_labels();
            reset_ac_max_labels();
        }
    }

    request_render_update_ = true;
}


void MainWindow::request_plot_buffer(const char* buffer_name)
{
    MessageComposer message_composer;
    message_composer.push(MessageType::PlotBufferRequest)
        .push(std::string(buffer_name))
        .send(&socket_);
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

    if (request_render_update_) {
        // Update visualization pane
        ui_->bufferPreview->update();

        request_render_update_ = false;
    }
}


void MainWindow::request_render_update()
{
    request_render_update_ = true;
}


void MainWindow::persist_settings()
{
    using BufferExpiration = QPair<QString, QDateTime>;

    QSettings settings(QSettings::Format::IniFormat,
                       QSettings::Scope::UserScope,
                       "gdbimagewatch");

    QList<BufferExpiration> persisted_session_buffers;

    // Load previous session symbols
    QList<BufferExpiration> previous_session_buffers_qlist =
        settings.value("PreviousSession/buffers")
            .value<QList<BufferExpiration>>();

    QDateTime now             = QDateTime::currentDateTime();
    QDateTime next_expiration = now.addDays(1);

    // Of the buffers not currently being visualized, only keep those whose
    // timer hasn't expired yet and is not in the set of removed names
    for (const auto& prev_buff : previous_session_buffers_qlist) {
        const string buff_name_std_str = prev_buff.first.toStdString();

        const bool being_viewed =
            held_buffers_.find(buff_name_std_str) != held_buffers_.end();
        const bool was_removed =
            removed_buffer_names_.find(buff_name_std_str) !=
            removed_buffer_names_.end();

        if (was_removed) {
            previous_session_buffers_.erase(buff_name_std_str);
        } else if (!being_viewed && prev_buff.second >= now) {
            persisted_session_buffers.append(prev_buff);
        }
    }

    for (const auto& held_buffer : held_buffers_) {
        persisted_session_buffers.append(
            BufferExpiration(held_buffer.first.c_str(), next_expiration));
    }

    // Write default suffix for buffer export
    settings.setValue("Export/default_export_suffix", default_export_suffix_);

    // Write maximum framerate
    settings.setValue("Rendering/maximum_framerate", render_framerate_);

    // Write previous session symbols
    settings.setValue("PreviousSession/buffers",
                      QVariant::fromValue(persisted_session_buffers));

    // Write window position/size
    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();

    settings.sync();

    removed_buffer_names_.clear();
}


vec4 MainWindow::get_stage_coordinates(float pos_window_x, float pos_window_y)
{
    GameObject* cam_obj = currently_selected_stage_->get_game_object("camera");
    Camera* cam         = cam_obj->get_component<Camera>("camera_component");

    GameObject* buffer_obj =
        currently_selected_stage_->get_game_object("buffer");
    Buffer* buffer = buffer_obj->get_component<Buffer>("buffer_component");

    float win_w = ui_->bufferPreview->width();
    float win_h = ui_->bufferPreview->height();
    vec4 mouse_pos_ndc(2.0f * (pos_window_x - win_w / 2) / win_w,
                       -2.0f * (pos_window_y - win_h / 2) / win_h,
                       0,
                       1);
    mat4 view      = cam_obj->get_pose().inv();
    mat4 buff_pose = buffer_obj->get_pose();
    mat4 vp_inv    = (cam->projection * view * buff_pose).inv();

    vec4 mouse_pos = vp_inv * mouse_pos_ndc;
    mouse_pos +=
        vec4(buffer->buffer_width_f / 2.f, buffer->buffer_height_f / 2.f, 0, 0);

    return mouse_pos;
}


void MainWindow::update_status_bar()
{
    if (currently_selected_stage_ != nullptr) {
        stringstream message;

        GameObject* cam_obj =
            currently_selected_stage_->get_game_object("camera");
        Camera* cam = cam_obj->get_component<Camera>("camera_component");

        GameObject* buffer_obj =
            currently_selected_stage_->get_game_object("buffer");
        Buffer* buffer = buffer_obj->get_component<Buffer>("buffer_component");

        float mouse_x = ui_->bufferPreview->mouse_x();
        float mouse_y = ui_->bufferPreview->mouse_y();

        vec4 mouse_pos = get_stage_coordinates(mouse_x, mouse_y);

        message << std::fixed << std::setprecision(3) << "("
                << static_cast<int>(floor(mouse_pos.x())) << ", "
                << static_cast<int>(floor(mouse_pos.y())) << ")\t"
                << cam->compute_zoom() * 100.0f << "%";
        message << " val=";

        buffer->get_pixel_info(
            message,
            static_cast<int>(floor(mouse_pos.x())),
            static_cast<int>(floor(mouse_pos.y())));

        status_bar_->setText(message.str().c_str());
    }
}


qreal MainWindow::get_screen_dpi_scale()
{
    return QGuiApplication::primaryScreen()->devicePixelRatio();
}


string MainWindow::get_type_label(BufferType type, int channels)
{
    stringstream result;
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
