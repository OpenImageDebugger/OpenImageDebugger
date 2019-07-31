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

#include "ui_main_window.h"
#include "visualization/components/camera.h"
#include "visualization/game_object.h"
#include "ipc/message_exchange.h"


using namespace std;


Q_DECLARE_METATYPE(QList<QString>)






//// TODO restructure
#include <QBuffer>
struct BufferRequestMessage
{
    std::shared_ptr<uint8_t> managed_buffer;
    std::string variable_name_str;
    std::string display_name_str;
    int width_i;
    int height_i;
    int channels;
    BufferType type;
    int step;
    std::string pixel_layout;
    bool transpose_buffer;

    BufferRequestMessage(const BufferRequestMessage& buff);

    ~BufferRequestMessage() {}

    BufferRequestMessage() {}

    std::shared_ptr<uint8_t> make_shared_buffer_object(const QByteArray& obj) {
        const int length = obj.size();
        uint8_t* copied_buffer = new uint8_t[length];
        for(int i = 0; i < length; ++i) {
            copied_buffer[i] = static_cast<uint8_t>(obj.constData()[i]);
        }

        return std::shared_ptr<uint8_t>(
                    copied_buffer, [](uint8_t* obj) { delete[] obj; });
    }
    std::shared_ptr<uint8_t> make_float_buffer_from_double(const double* buff, int length) {
        std::shared_ptr<uint8_t> result(
                    reinterpret_cast<uint8_t*>(new float[length]),
                    [](uint8_t* buff) { delete[] reinterpret_cast<float*>(buff); });

        // Cast from double to float
        float* dst = reinterpret_cast<float*>(result.get());
        for (int i = 0; i < length; ++i) {
            dst[i] = static_cast<float>(buff[i]);
        }

        return result;
    }

    void send_buffer_plot_request(QTcpSocket& socket) {
        // TODO write
    }
    bool retrieve_buffer_plot_request(QTcpSocket& socket) {
        return false;
        FILE* xow=fopen("/tmp/tst.log", "w");
        bool status = false;

        if(socket.bytesAvailable() == 0) {
            fclose(xow);
            return false;
        }
        fprintf(xow, "tem umas msgs pra mim...\n");

        QDataStream data_stream(&socket);
        data_stream.setVersion(QDataStream::Qt_4_0);
        QString test;

        do {
            data_stream.startTransaction();
            data_stream >> test;
            fprintf(xow, "Got msg: [%s]\n", test.toStdString().c_str());
        } while(!data_stream.commitTransaction());
        fclose(xow);

        // TODO fill remaining fields
        /*
        QByteArray buffer_content;

        if (type == BufferType::Float64) {
            managed_buffer = make_float_buffer_from_double(
                        reinterpret_cast<const double*>(buffer_content.constData()),
                        width_i * height_i * channels);
        } else {
            managed_buffer = make_shared_buffer_object(buffer_content.constData());
        }
        */

        return status;
    }

    BufferRequestMessage& operator=(const BufferRequestMessage&) = delete;

    /**
     * Returns buffer width taking into account its transposition flag
     */
    int get_visualized_width() const {
        if (!transpose_buffer) {
            return width_i;
        } else {
            return height_i;
        }
    }

    /**
     * Returns buffer height taking into account its transposition flag
     */
    int get_visualized_height() const {
        if (!transpose_buffer) {
            return height_i;
        } else {
            return width_i;
        }
    }
};
////////////////






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
    // TODO get rid of this function
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


void MainWindow::decode_incoming_messages() {
    FILE* xow=fopen("/tmp/tst.log", "w");

    available_vars_.clear();

    const auto decode_set_available_symbols = [&]() {
        std::unique_lock<std::mutex> lock(ui_mutex_);
        size_t number_symbols;

        socket_.read(reinterpret_cast<char*>(&number_symbols),
                     static_cast<qint64>(sizeof(number_symbols)));

        for (int s = 0; s < number_symbols; ++s) {
            size_t symbol_length;
            socket_.read(reinterpret_cast<char*>(&symbol_length),
                         static_cast<qint64>(sizeof(symbol_length)));

            std::vector<char> symbol_value(symbol_length + 1, '\0');
            socket_.read(symbol_value.data(),
                         static_cast<qint64>(symbol_length));

            // Add symbol name to autocomplete list
            available_vars_.push_back(symbol_value.data());

            // Plot buffer if it was available in the previous session
            if (previous_session_buffers_.find(symbol_value.data()) !=
                previous_session_buffers_.end()) {
                plot_callback_(symbol_value.data());
            }
        }
    };

    if(socket_.bytesAvailable() == 0) {
        fclose(xow);
        return;
    }
    fprintf(xow, "tem umas msgs pra mim...\n");

    MessageType header;
    if(!socket_.read(reinterpret_cast<char*>(&header),
                     static_cast<qint64>(sizeof(header)))) {
        return;
    }

    switch(header) {
    case MessageType::SetAvailableSymbols:
        decode_set_available_symbols();
        break;
    }

    fclose(xow);

    completer_updated_ = true;
}


void MainWindow::loop()
{
    // Buffer icon dimensions
    QSizeF icon_size         = get_icon_size();
    int icon_width           = icon_size.width();
    int icon_height          = icon_size.height();
    const int bytes_per_line = icon_width * 3;

    decode_incoming_messages();

    // Handle buffer plot requests
    BufferRequestMessage request;
    if (request.retrieve_buffer_plot_request(socket_))
    {

        auto buffer_stage = stages_.find(request.variable_name_str);
        held_buffers_[request.variable_name_str] = request.managed_buffer;

        if (buffer_stage == stages_.end()) { // New buffer request
            shared_ptr<Stage> stage = make_shared<Stage>(this);
            if (!stage->initialize(request.managed_buffer.get(),
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
            buffer_stage->second->buffer_update(request.managed_buffer.get(),
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

        message << std::fixed << std::setprecision(3) << "("
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
