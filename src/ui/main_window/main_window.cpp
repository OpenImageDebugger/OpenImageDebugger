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

#include <QAction>
#include <QDateTime>
#include <QScreen>
#include <QSettings>

#include "main_window.h"

#include "debuggerinterface/managed_pointer.h"
#include "debuggerinterface/python_native_interface.h"
#include "ui_main_window.h"
#include "visualization/components/camera.h"
#include "visualization/game_object.h"


using namespace std;


Q_DECLARE_METATYPE(QList<QString>)


MainWindow::MainWindow(QWidget* parent)
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
    , plot_callback_(nullptr)
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
    update_timer_.start(1000.0 / render_framerate_);
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


void MainWindow::set_plot_callback(int (*plot_cbk)(const char*))
{
    plot_callback_ = plot_cbk;
}


void MainWindow::plot_buffer(const BufferRequestMessage& buffer_metadata)
{
    std::unique_lock<std::mutex> lock(ui_mutex_);
    pending_updates_.push_back(buffer_metadata);
}


deque<string> MainWindow::get_observed_symbols()
{
    deque<string> observed_names;

    for (const auto& name : held_buffers_) {
        observed_names.push_back(name.first);
    }

    return observed_names;
}


bool MainWindow::is_window_ready()
{
    return ui_->bufferPreview->is_ready() && is_window_ready_;
}


void MainWindow::set_available_symbols(const deque<string>& available_vars)
{
    std::unique_lock<std::mutex> lock(ui_mutex_);

    available_vars_.clear();

    for (const auto& var_name : available_vars) {
        // Add symbol name to autocomplete list
        available_vars_.push_back(var_name.c_str());

        // Plot buffer if it was available in the previous session
        if (previous_session_buffers_.find(var_name) !=
            previous_session_buffers_.end()) {
            plot_callback_(var_name.c_str());
        }
    }

    completer_updated_ = true;
}


void MainWindow::loop()
{
    // Buffer icon dimensions
    QSizeF icon_size         = get_icon_size();
    int icon_width           = icon_size.width();
    int icon_height          = icon_size.height();
    const int bytes_per_line = icon_width * 3;

    // Handle buffer plot requests
    while (!pending_updates_.empty()) {
        const BufferRequestMessage& request = pending_updates_.front();

        uint8_t* srcBuffer;
        shared_ptr<uint8_t> managedBuffer;
        if (request.type == Buffer::BufferType::Float64) {
            managedBuffer = make_float_buffer_from_double(
                static_cast<double*>(
                    get_c_ptr_from_py_buffer(request.py_buffer)),
                request.width_i * request.height_i * request.channels);
            srcBuffer = managedBuffer.get();
        } else {
            managedBuffer = make_shared_py_object(request.py_buffer);
            srcBuffer     = static_cast<uint8_t*>(
                get_c_ptr_from_py_buffer(request.py_buffer));
        }

        auto buffer_stage = stages_.find(request.variable_name_str);
        held_buffers_[request.variable_name_str] = managedBuffer;

        if (buffer_stage == stages_.end()) { // New buffer request
            shared_ptr<Stage> stage = make_shared<Stage>(this);
            if (!stage->initialize(srcBuffer,
                                   request.width_i,
                                   request.height_i,
                                   request.channels,
                                   request.type,
                                   request.step,
                                   request.pixel_layout,
                                   request.transpose_buffer)) {
                cerr << "[error] Could not initialize opengl canvas!" << endl;
            }
            stage->contrast_enabled            = ac_enabled_;
            stages_[request.variable_name_str] = stage;

            ui_->bufferPreview->render_buffer_icon(
                stage.get(), icon_width, icon_height);

            QImage bufferIcon(stage->buffer_icon.data(),
                              icon_width,
                              icon_height,
                              bytes_per_line,
                              QImage::Format_RGB888);

            stringstream label;
            label << request.display_name_str << "\n["
                  << request.get_visualized_width() << "x"
                  << request.get_visualized_height() << "]\n"
                  << get_type_label(request.type, request.channels);
            QListWidgetItem* item =
                new QListWidgetItem(QPixmap::fromImage(bufferIcon),
                                    label.str().c_str(),
                                    ui_->imageList);
            item->setData(Qt::UserRole,
                          QString(request.variable_name_str.c_str()));
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                           Qt::ItemIsDragEnabled);
            ui_->imageList->addItem(item);

            persist_settings_deferred();
        } else { // Update buffer request
            buffer_stage->second->buffer_update(srcBuffer,
                                                request.width_i,
                                                request.height_i,
                                                request.channels,
                                                request.type,
                                                request.step,
                                                request.pixel_layout,
                                                request.transpose_buffer);
            // Update buffer icon
            shared_ptr<Stage>& stage = stages_[request.variable_name_str];
            ui_->bufferPreview->render_buffer_icon(
                stage.get(), icon_width, icon_height);

            // Looking for corresponding item...
            QImage bufferIcon(stage->buffer_icon.data(),
                              icon_width,
                              icon_height,
                              bytes_per_line,
                              QImage::Format_RGB888);
            stringstream label;
            label << request.display_name_str << "\n["
                  << request.get_visualized_width() << "x"
                  << request.get_visualized_height() << "]\n"
                  << get_type_label(request.type, request.channels);

            for (int i = 0; i < ui_->imageList->count(); ++i) {
                QListWidgetItem* item = ui_->imageList->item(i);
                if (item->data(Qt::UserRole) ==
                    request.variable_name_str.c_str()) {
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

        pending_updates_.pop_front();
        request_render_update_ = true;
    }

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
    vec4 mouse_pos_ndc(2.0 * (pos_window_x - win_w / 2) / win_w,
                       -2.0 * (pos_window_y - win_h / 2) / win_h,
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

        message << std::fixed << std::setprecision(1) << "("
                << static_cast<int>(floor(mouse_pos.x())) << ", "
                << static_cast<int>(floor(mouse_pos.y())) << ")\t"
                << cam->compute_zoom() * 100.0 << "%";
        message << " val=";

        buffer->get_pixel_info(
            message, floor(mouse_pos.x()), floor(mouse_pos.y()));

        status_bar_->setText(message.str().c_str());
    }
}


qreal MainWindow::get_screen_dpi_scale()
{
    return QGuiApplication::primaryScreen()->devicePixelRatio();
}


string MainWindow::get_type_label(Buffer::BufferType type, int channels)
{
    stringstream result;
    if (type == Buffer::BufferType::Float32) {
        result << "float32";
    } else if (type == Buffer::BufferType::UnsignedByte) {
        result << "uint8";
    } else if (type == Buffer::BufferType::Short) {
        result << "int16";
    } else if (type == Buffer::BufferType::UnsignedShort) {
        result << "uint16";
    } else if (type == Buffer::BufferType::Int32) {
        result << "int32";
    } else if (type == Buffer::BufferType::Float64) {
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
