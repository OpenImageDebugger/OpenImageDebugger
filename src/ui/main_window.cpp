#include <iomanip>
#include <sstream>

#include <QAction>
#include <QFileDialog>
#include <QSettings>
#include <QShortcut>
#include <QStandardPaths>
#include <QFontDatabase>
#include <QDebug>
#include <QScreen>

#include "main_window.h"

#include "debuggerinterface/python_native_interface.h"
#include "ui_main_window.h"
#include "debuggerinterface/managed_pointer.h"
#include "io/buffer_exporter.h"
#include "visualization/game_object.h"
#include "visualization/components/camera.h"

Q_DECLARE_METATYPE(QList<QString>)

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    is_window_ready_(false),
    request_render_update_(true),
    currently_selected_stage_(nullptr),
    completer_updated_(false),
    ui_(new Ui::MainWindowUi),
    ac_enabled_(true),
    link_views_enabled_(false),
    plot_callback_(nullptr)
{
    ui_->setupUi(this);

    initialize_toolbar_icons();

    connect(&settings_persist_timer_, SIGNAL(timeout()), this, SLOT(persist_settings_impl()));
    settings_persist_timer_.setSingleShot(true);
    connect(&update_timer_, SIGNAL(timeout()), this, SLOT(loop()));

    symbol_list_focus_shortcut_ = new QShortcut(QKeySequence(Qt::CTRL|Qt::Key_K), this);
    connect(symbol_list_focus_shortcut_, SIGNAL(activated()), ui_->symbolList, SLOT(setFocus()));

    connect(ui_->imageList, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), this, SLOT(buffer_selected(QListWidgetItem*)));
    buffer_removal_shortcut_ = new QShortcut(QKeySequence(Qt::Key_Delete), ui_->imageList);
    connect(buffer_removal_shortcut_, SIGNAL(activated()), this, SLOT(remove_selected_buffer()));

    connect(ui_->symbolList, SIGNAL(editingFinished()), this, SLOT(symbol_selected()));

    ui_->bufferPreview->set_main_window(this);

    // Configure symbol completer
    symbol_completer_ = new SymbolCompleter(this);
    symbol_completer_->setCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
    symbol_completer_->setCompletionMode(QCompleter::PopupCompletion);
    symbol_completer_->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    ui_->symbolList->setCompleter(symbol_completer_);
    connect(ui_->symbolList->completer(), SIGNAL(activated(QString)), this, SLOT(symbol_completed(QString)));

    // Configure auto contrast inputs
    ui_->ac_red_min->setValidator(   new QDoubleValidator(ui_->ac_red_min) );
    ui_->ac_green_min->setValidator( new QDoubleValidator(ui_->ac_green_min) );
    ui_->ac_blue_min->setValidator(  new QDoubleValidator(ui_->ac_blue_min) );

    ui_->ac_red_max->setValidator(   new QDoubleValidator(ui_->ac_red_max) );
    ui_->ac_green_max->setValidator( new QDoubleValidator(ui_->ac_green_max) );
    ui_->ac_blue_max->setValidator(  new QDoubleValidator(ui_->ac_blue_max) );

    connect(ui_->ac_red_min, SIGNAL(editingFinished()), this, SLOT(ac_red_min_update()));
    connect(ui_->ac_red_max, SIGNAL(editingFinished()), this, SLOT(ac_red_max_update()));
    connect(ui_->ac_green_min, SIGNAL(editingFinished()), this, SLOT(ac_green_min_update()));
    connect(ui_->ac_green_max, SIGNAL(editingFinished()), this, SLOT(ac_green_max_update()));
    connect(ui_->ac_blue_min, SIGNAL(editingFinished()), this, SLOT(ac_blue_min_update()));
    connect(ui_->ac_blue_max, SIGNAL(editingFinished()), this, SLOT(ac_blue_max_update()));
    connect(ui_->ac_alpha_min, SIGNAL(editingFinished()), this, SLOT(ac_alpha_min_update()));
    connect(ui_->ac_alpha_max, SIGNAL(editingFinished()), this, SLOT(ac_alpha_max_update()));

    connect(ui_->ac_reset_min, SIGNAL(clicked()), this, SLOT(ac_min_reset()));
    connect(ui_->ac_reset_max, SIGNAL(clicked()), this, SLOT(ac_max_reset()));

    connect(ui_->acToggle, SIGNAL(clicked()), this, SLOT(ac_toggle()));

    connect(ui_->reposition_buffer, SIGNAL(clicked()), this, SLOT(recenter_buffer()));

    connect(ui_->linkViewsToggle, SIGNAL(clicked()), this, SLOT(link_views_toggle()));

    connect(ui_->rotate_90_cw, SIGNAL(clicked()), this, SLOT(rotate_90_cw()));
    connect(ui_->rotate_90_ccw, SIGNAL(clicked()), this, SLOT(rotate_90_ccw()));

    status_bar_ = new QLabel(this);
    status_bar_->setAlignment(Qt::AlignRight);
    statusBar()->addWidget(status_bar_, 1);

    ui_->imageList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui_->imageList, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(show_context_menu(const QPoint&)));

    load_settings();

    is_window_ready_ = true;
}

void MainWindow::load_settings() {
    qRegisterMetaTypeStreamOperators<QList<QString> >("QList<QString>");

    QSettings settings(QSettings::Format::IniFormat,
                       QSettings::Scope::UserScope,
                       "gdbimagewatch");

    settings.sync();

    // Load maximum framerate
    render_framerate_ = settings.value("Rendering/maximum_framerate", 60)
                                .value<double>();
    if(render_framerate_ <= 0.f) {
        render_framerate_ = 1.0;
    }

    // Load previous session symbols
    QList<QString> buffers = settings.value("PreviousSession/buffers")
                                     .value<QList<QString> >();

    for(const auto& i: buffers) {
        previous_session_buffers_.insert(i.toStdString());
    }

    // Load window position/size
    settings.beginGroup("MainWindow");
    resize(settings.value("size", size()).toSize());
    move(settings.value("pos", pos()).toPoint());
    settings.endGroup();
}

void MainWindow::persist_settings_impl() {
    QSettings settings(QSettings::Format::IniFormat,
                       QSettings::Scope::UserScope,
                       "gdbimagewatch");

    QList<QString> currentSessionBuffers;

    for(const auto& held_buffer: held_buffers_) {
        currentSessionBuffers.append(held_buffer.first.c_str());
    }

    // Write maximum framerate
    settings.setValue("Rendering/maximum_framerate",
                      render_framerate_);

    // Write previous session symbols
    settings.setValue("PreviousSession/buffers",
                      QVariant::fromValue(currentSessionBuffers));

    // Write window position/size
    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();

    settings.sync();
}

void MainWindow::persist_settings() {
    settings_persist_timer_.start(100);
}

MainWindow::~MainWindow()
{
    held_buffers_.clear();
    is_window_ready_ = false;

    delete ui_;
}

void MainWindow::show() {
    update_timer_.start(1000.0 / render_framerate_);
    QMainWindow::show();
}

void MainWindow::draw()
{
    if(currently_selected_stage_ != nullptr) {
        currently_selected_stage_->draw();
    }
}

void MainWindow::resize_callback(int w, int h)
{
    for(auto& stage: stages_)
        stage.second->resize_callback(w, h);
}

void MainWindow::scroll_callback(float delta)
{
    if(link_views_enabled_) {
        for(auto& stage: stages_) {
            stage.second->scroll_callback(delta);
        }
    } else if(currently_selected_stage_ != nullptr) {
        currently_selected_stage_->scroll_callback(delta);
    }

    update_statusbar();

    request_render_update_ = true;
}

void enableInputs(const initializer_list<QLineEdit*>& inputs) {
    for(auto& input: inputs) {
        input->setEnabled(true);
    }
}

void disableInputs(const initializer_list<QLineEdit*>& inputs) {
    for(auto& input: inputs) {
        input->setEnabled(false);
        input->setText("");
    }
}

void MainWindow::reset_ac_min_labels()
{
    GameObject* buffer_obj = currently_selected_stage_->getGameObject("buffer");
    Buffer* buffer = buffer_obj->getComponent<Buffer>("buffer_component");
    float* ac_min = buffer->min_buffer_values();

    ui_->ac_red_min->setText(QString::number(ac_min[0]));

    if(buffer->channels == 4) {
        enableInputs({ui_->ac_green_min, ui_->ac_blue_min, ui_->ac_alpha_min});

        ui_->ac_green_min->setText(QString::number(ac_min[1]));
        ui_->ac_blue_min->setText(QString::number(ac_min[2]));
        ui_->ac_alpha_min->setText(QString::number(ac_min[3]));
    }
    else if(buffer->channels == 3) {
        enableInputs({ui_->ac_green_min, ui_->ac_blue_min});
        ui_->ac_alpha_min->setEnabled(false);

        ui_->ac_green_min->setText(QString::number(ac_min[1]));
        ui_->ac_blue_min->setText(QString::number(ac_min[2]));
    }
    else if(buffer->channels == 2) {
        ui_->ac_green_min->setEnabled(true);
        disableInputs({ui_->ac_blue_min, ui_->ac_alpha_min});

        ui_->ac_green_min->setText(QString::number(ac_min[1]));
    } else {
        disableInputs({ui_->ac_green_min, ui_->ac_blue_min, ui_->ac_alpha_min});
    }
}

void MainWindow::reset_ac_max_labels()
{
    GameObject* buffer_obj = currently_selected_stage_->getGameObject("buffer");
    Buffer* buffer = buffer_obj->getComponent<Buffer>("buffer_component");
    float* ac_max = buffer->max_buffer_values();

    ui_->ac_red_max->setText(QString::number(ac_max[0]));
    if(buffer->channels == 4) {
        enableInputs({ui_->ac_green_max, ui_->ac_blue_max, ui_->ac_alpha_max});

        ui_->ac_green_max->setText(QString::number(ac_max[1]));
        ui_->ac_blue_max->setText(QString::number(ac_max[2]));
        ui_->ac_alpha_max->setText(QString::number(ac_max[3]));
    }
    else if(buffer->channels == 3) {
        enableInputs({ui_->ac_green_max, ui_->ac_blue_max});
        ui_->ac_alpha_max->setEnabled(false);

        ui_->ac_green_max->setText(QString::number(ac_max[1]));
        ui_->ac_blue_max->setText(QString::number(ac_max[2]));
    }
    else if(buffer->channels == 2) {
        ui_->ac_green_max->setEnabled(true);
        disableInputs({ui_->ac_blue_max, ui_->ac_alpha_max});

        ui_->ac_green_max->setText(QString::number(ac_max[1]));
    } else {
        disableInputs({ui_->ac_green_max, ui_->ac_blue_max, ui_->ac_alpha_max});
    }
}

void MainWindow::mouse_drag_event(int mouse_x, int mouse_y)
{
    const qreal screen_dpi_scale = get_screen_dpi_scale();
    const QPoint virtual_motion(mouse_x * screen_dpi_scale,
                                mouse_y * screen_dpi_scale);

    if(link_views_enabled_) {
        for(auto& stage: stages_)
            stage.second->mouse_drag_event(virtual_motion.x(),
                                           virtual_motion.y());
    } else if(currently_selected_stage_ != nullptr) {
        currently_selected_stage_->mouse_drag_event(virtual_motion.x(),
                                                    virtual_motion.y());
    }

    request_render_update_ = true;
}

void MainWindow::mouse_move_event(int, int)
{
    update_statusbar();
}

void MainWindow::plot_buffer(const BufferRequestMessage& buffer_metadata)
{
    std::unique_lock<std::mutex> lock(ui_mutex_);
    pending_updates_.push_back(buffer_metadata);
}

void MainWindow::loop() {
    const qreal screen_dpi_scale = get_screen_dpi_scale();
    // Buffer icon dimensions
    const int icon_width = 100 * screen_dpi_scale;
    const int icon_height = 50 * screen_dpi_scale;
    const int bytes_per_line = icon_width * 3;

    while(!pending_updates_.empty()) {
        const BufferRequestMessage& request = pending_updates_.front();

        uint8_t* srcBuffer;
        shared_ptr<uint8_t> managedBuffer;
        if(request.type == Buffer::BufferType::Float64) {
            managedBuffer = makeFloatBufferFromDouble(static_cast<double*>(getCPtrFromPyBuffer(request.py_buffer)),
                                                      request.width_i * request.height_i * request.channels);
            srcBuffer = managedBuffer.get();
        } else {
            managedBuffer = makeSharedPyObject(request.py_buffer);
            srcBuffer = static_cast<uint8_t*>(getCPtrFromPyBuffer(request.py_buffer));
        }

        auto buffer_stage = stages_.find(request.variable_name_str);
        held_buffers_[request.variable_name_str] = managedBuffer;
        if(buffer_stage == stages_.end()) {
            // New buffer request
            shared_ptr<Stage> stage = make_shared<Stage>();
            if(!stage->initialize(ui_->bufferPreview,
                                  srcBuffer,
                                  request.width_i,
                                  request.height_i,
                                  request.channels,
                                  request.type,
                                  request.step,
                                  request.pixel_layout,
                                  ac_enabled_)) {
                cerr << "[error] Could not initialize opengl canvas!"<<endl;
            }
            stages_[request.variable_name_str] = stage;

            ui_->bufferPreview->render_buffer_icon(stage.get(),
                                                   icon_width, icon_height);

            QImage bufferIcon(stage->buffer_icon_.data(), icon_width,
                              icon_height, bytes_per_line, QImage::Format_RGB888);

            stringstream label;
            label << request.display_name_str << "\n[" << request.width_i << "x" <<
                     request.height_i << "]\n" <<
                     get_type_label(request.type, request.channels);
            QListWidgetItem* item = new QListWidgetItem(QPixmap::fromImage(bufferIcon),
                                                        label.str().c_str(),
                                                        ui_->imageList);
            item->setData(Qt::UserRole, QString(request.variable_name_str.c_str()));
            item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsDragEnabled);
            ui_->imageList->addItem(item);

            persist_settings();
        } else {
            buffer_stage->second->buffer_update(srcBuffer,
                                                request.width_i,
                                                request.height_i,
                                                request.channels,
                                                request.type,
                                                request.step,
                                                request.pixel_layout);
            // Update buffer icon
            shared_ptr<Stage>& stage = stages_[request.variable_name_str];
            ui_->bufferPreview->render_buffer_icon(stage.get(),
                                                   icon_width, icon_height);

            // Looking for corresponding item...
            QImage bufferIcon(stage->buffer_icon_.data(), icon_width,
                              icon_height, bytes_per_line, QImage::Format_RGB888);
            stringstream label;
            label << request.display_name_str << "\n[" << request.width_i << "x" <<
                     request.height_i << "]\n" <<
                     get_type_label(request.type, request.channels);

            for(int i = 0; i < ui_->imageList->count(); ++i) {
                QListWidgetItem* item = ui_->imageList->item(i);
                if(item->data(Qt::UserRole) == request.variable_name_str.c_str()) {
                    item->setIcon(QPixmap::fromImage(bufferIcon));
                    item->setText(label.str().c_str());
                    break;
                }
            }

            // Update AC values
            if(currently_selected_stage_ != nullptr) {
                reset_ac_min_labels();
                reset_ac_max_labels();
            }
        }

        pending_updates_.pop_front();
        request_render_update_ = true;
    }

    if(completer_updated_) {
        symbol_completer_->updateSymbolList(available_vars_);
        completer_updated_ = false;
    }

    if(request_render_update_) {
        if(currently_selected_stage_ != nullptr) {
            currently_selected_stage_->update();
        }

        ui_->bufferPreview->updateGL();

        request_render_update_ = false;
    }
}

void MainWindow::buffer_selected(QListWidgetItem * item) {
    if(item == nullptr)
        return;

    auto stage = stages_.find(item->data(Qt::UserRole).toString().toStdString());
    if(stage != stages_.end()) {
        set_currently_selected_stage(stage->second.get());
        reset_ac_min_labels();
        reset_ac_max_labels();

        update_statusbar();
    }
}

void MainWindow::ac_red_min_update()
{
    set_ac_min_value(0, ui_->ac_red_min->text().toFloat());
}

void MainWindow::ac_green_min_update()
{
    set_ac_min_value(1, ui_->ac_green_min->text().toFloat());
}

void MainWindow::ac_blue_min_update()
{
    set_ac_min_value(2, ui_->ac_blue_min->text().toFloat());
}

void MainWindow::ac_alpha_min_update()
{
    set_ac_min_value(3, ui_->ac_alpha_min->text().toFloat());
}

void MainWindow::set_ac_min_value(int idx, float value)
{
   if(currently_selected_stage_ != nullptr) {
       GameObject* buffer_obj = currently_selected_stage_->getGameObject("buffer");
       Buffer* buff = buffer_obj->getComponent<Buffer>("buffer_component");
       buff->min_buffer_values()[idx] = value;
       buff->computeContrastBrightnessParameters();

       request_render_update_ = true;
   }
}

void MainWindow::set_ac_max_value(int idx, float value)
{
   if(currently_selected_stage_ != nullptr) {
       GameObject* buffer_obj = currently_selected_stage_->getGameObject("buffer");
       Buffer* buff = buffer_obj->getComponent<Buffer>("buffer_component");
       buff->max_buffer_values()[idx] = value;
       buff->computeContrastBrightnessParameters();

       request_render_update_ = true;
   }
}

void MainWindow::update_statusbar()
{
    if(currently_selected_stage_ != nullptr) {
        stringstream message;
        GameObject* cam_obj = currently_selected_stage_->getGameObject("camera");
        Camera* cam = cam_obj->getComponent<Camera>("camera_component");

        GameObject* buffer_obj = currently_selected_stage_->getGameObject("buffer");
        Buffer* buffer = buffer_obj->getComponent<Buffer>("buffer_component");

        float mouseX = ui_->bufferPreview->mouseX();
        float mouseY = ui_->bufferPreview->mouseY();
        float winW = ui_->bufferPreview->width();
        float winH = ui_->bufferPreview->height();
        vec4 mouse_pos_ndc( 2.0*(mouseX-winW/2)/winW, -2.0*(mouseY-winH/2)/winH, 0,1);
        mat4 view = cam_obj->get_pose().inv();
        mat4 buffRot = mat4::rotation(buffer_obj->angle);
        mat4 vp_inv = (cam->projection*view*buffRot).inv();

        vec4 mouse_pos = vp_inv * mouse_pos_ndc;
        mouse_pos += vec4(buffer->buffer_width_f/2.f,
                          buffer->buffer_height_f/2.f, 0, 0);

        message << std::fixed << std::setprecision(1) <<
                   "(" << floorf(mouse_pos.x()) << "," << floorf(mouse_pos.y()) << ")\t" <<
                   cam->get_zoom() * 100.0 << "%";
        message << " val=";
        buffer->getPixelInfo(message, floor(mouse_pos.x()), floor(mouse_pos.y()));
        status_bar_->setText(message.str().c_str());
    }
}

void MainWindow::initialize_toolbar_icons() {
#define SET_FONT_ICON(ui_element, unicode_id) \
    ui_element->setFont(icons_font); \
    ui_element->setText(unicode_id);

#define SET_VECTOR_ICON(ui_element, icon_file_name, width, height) \
    ui_element->setScaledContents(true); \
    ui_element->setMinimumWidth(std::round(width)); \
    ui_element->setMaximumWidth(std::round(width)); \
    ui_element->setMinimumHeight(std::round(height)); \
    ui_element->setMaximumHeight(std::round(height)); \
    ui_element->setPixmap( \
        QIcon(":/resources/icons/" icon_file_name). \
            pixmap(QSize(std::round(width * screen_dpi_scale), \
                         std::round(height * screen_dpi_scale)))); \
    ui_element->setAlignment(Qt::AlignRight|Qt::AlignVCenter);

    if(QFontDatabase::addApplicationFont(":/resources/icons/fontello.ttf") < 0) {
        qWarning() << "Could not load ionicons font file!";
    }

    qreal screen_dpi_scale = get_screen_dpi_scale();
    QFont icons_font;
    icons_font.setFamily("fontello");
    icons_font.setPointSizeF(10.f);

    SET_FONT_ICON(ui_->acEdit, "\ue803");
    SET_FONT_ICON(ui_->acToggle, "\ue804");
    SET_FONT_ICON(ui_->reposition_buffer, "\ue800");
    SET_FONT_ICON(ui_->linkViewsToggle, "\ue805");
    SET_FONT_ICON(ui_->rotate_90_cw, "\ue801");
    SET_FONT_ICON(ui_->rotate_90_ccw, "\ue802");
    SET_FONT_ICON(ui_->ac_reset_min, "\ue808");
    SET_FONT_ICON(ui_->ac_reset_max, "\ue808");

    SET_FONT_ICON(ui_->label_min, "\ue806");
    SET_FONT_ICON(ui_->label_max, "\ue807");

    SET_VECTOR_ICON(ui_->label_red_min, "label_red_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_red_max, "label_red_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_green_min, "label_green_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_green_max, "label_green_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_blue_min, "label_blue_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_blue_max, "label_blue_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_alpha_min, "label_alpha_channel.svg", 10, 10);
    SET_VECTOR_ICON(ui_->label_alpha_max, "label_alpha_channel.svg", 10, 10);

    QLabel *label_min_max = new QLabel(ui_->minMaxEditor);
    SET_VECTOR_ICON(label_min_max, "lower_upper_bound.svg", 12.f, 52.f);
    ui_->gridLayout->addWidget(label_min_max, 0, 0, 2, 1);
}

qreal MainWindow::get_screen_dpi_scale() {
    return QGuiApplication::primaryScreen()->devicePixelRatio();
}

string MainWindow::get_type_label(Buffer::BufferType type, int channels)
{
    stringstream result;
    if(type == Buffer::BufferType::Float32) {
        result << "float32";
    } else if(type == Buffer::BufferType::UnsignedByte) {
        result << "uint8";
    } else if(type == Buffer::BufferType::Short) {
        result << "int16";
    } else if(type == Buffer::BufferType::UnsignedShort) {
        result << "uint16";
    } else if(type == Buffer::BufferType::Int32) {
        result << "int32";
    } else if(type == Buffer::BufferType::Float64) {
        result << "float64";
    }
    result << "x" << channels;

    return result.str();
}

void MainWindow::ac_red_max_update()
{
    set_ac_max_value(0, ui_->ac_red_max->text().toFloat());
}

void MainWindow::ac_green_max_update()
{
    set_ac_max_value(1, ui_->ac_green_max->text().toFloat());
}

void MainWindow::ac_blue_max_update()
{
    set_ac_max_value(2, ui_->ac_blue_max->text().toFloat());
}

void MainWindow::ac_alpha_max_update()
{
    set_ac_max_value(3, ui_->ac_alpha_max->text().toFloat());
}

void MainWindow::ac_min_reset()
{
   if(currently_selected_stage_ != nullptr) {
       GameObject* buffer_obj = currently_selected_stage_->getGameObject("buffer");
       Buffer* buff = buffer_obj->getComponent<Buffer>("buffer_component");
       buff->recomputeMinColorValues();
       buff->computeContrastBrightnessParameters();

       // Update inputs
       reset_ac_min_labels();

       request_render_update_ = true;
   }
}

void MainWindow::ac_max_reset()
{
   if(currently_selected_stage_ != nullptr) {
       GameObject* buffer_obj = currently_selected_stage_->getGameObject("buffer");
       Buffer* buff = buffer_obj->getComponent<Buffer>("buffer_component");
       buff->recomputeMaxColorValues();
       buff->computeContrastBrightnessParameters();

       // Update inputs
       reset_ac_max_labels();

       request_render_update_ = true;
   }
}

void MainWindow::ac_toggle()
{
    ac_enabled_ = !ac_enabled_;
    for(auto& stage: stages_)
        stage.second->contrast_enabled = ac_enabled_;

    request_render_update_ = true;
}

void MainWindow::recenter_buffer()
{
    if(link_views_enabled_) {
        for(auto& stage: stages_) {
            GameObject* cam_obj = stage.second->getGameObject("camera");
            Camera* cam = cam_obj->getComponent<Camera>("camera_component");
            cam->recenter_camera();
        }
    } else {
        if(currently_selected_stage_ != nullptr) {
            GameObject* cam_obj = currently_selected_stage_->getGameObject("camera");
            Camera* cam = cam_obj->getComponent<Camera>("camera_component");
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
    // TODO make all these events available to components
    if(link_views_enabled_) {
        for(auto& stage: stages_) {
            GameObject* buff_obj = stage.second->getGameObject("buffer");
            buff_obj->angle += 90.f * M_PI / 180.f;
        }
    } else {
        if(currently_selected_stage_ != nullptr) {
            GameObject* buff_obj = currently_selected_stage_->getGameObject("buffer");
            buff_obj->angle += 90.f * M_PI / 180.f;
        }
    }

    request_render_update_ = true;
}

void MainWindow::rotate_90_ccw()
{
    if(link_views_enabled_) {
        for(auto& stage: stages_) {
            GameObject* buff_obj = stage.second->getGameObject("buffer");
            buff_obj->angle -= 90.f * M_PI / 180.f;
        }
    } else {
        if(currently_selected_stage_ != nullptr) {
            GameObject* buff_obj = currently_selected_stage_->getGameObject("buffer");
            buff_obj->angle -= 90.f * M_PI / 180.f;
        }
    }

    request_render_update_ = true;
}

void MainWindow::set_currently_selected_stage(Stage* stage)
{
    currently_selected_stage_ = stage;
    request_render_update_ = true;
}

void MainWindow::remove_selected_buffer()
{
    if(ui_->imageList->count() > 0 && currently_selected_stage_ != nullptr) {
        QListWidgetItem* removedItem = ui_->imageList->takeItem(ui_->imageList->currentRow());
        string bufferName = removedItem->data(Qt::UserRole).toString().toStdString();
        stages_.erase(bufferName);
        held_buffers_.erase(bufferName);
        delete removedItem;

        if(stages_.size() == 0) {
            set_currently_selected_stage(nullptr);
        }

        persist_settings();
    }
}

void MainWindow::set_available_symbols(const deque<string>& available_vars)
{
    std::unique_lock<std::mutex> lock(ui_mutex_);

    available_vars_.clear();

    for(const auto& var_name: available_vars) {
        /*
         * Add symbol name to autocomplete list
         */
        available_vars_.push_back(var_name.c_str());

        // Plot buffer if it was available in the previous session
        if(previous_session_buffers_.find(var_name) !=
           previous_session_buffers_.end())
        {
            plot_callback_(var_name.c_str());
        }
    }

    completer_updated_ = true;
}

void MainWindow::symbol_selected() {
    QByteArray symbol_name_qba = ui_->symbolList->text().toLocal8Bit();
    const char* symbol_name = symbol_name_qba.constData();
    if(ui_->symbolList->text().length() > 0) {
        plot_callback_(symbol_name);
        // Clear symbol input
        ui_->symbolList->setText("");
    }
}

void MainWindow::symbol_completed(QString str) {
    if(str.length() > 0) {
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

    auto stage = stages_.find(sender_action->data().toString().toStdString())->second;
    GameObject* buffer_obj = stage->getGameObject("buffer");
    Buffer* component = buffer_obj->getComponent<Buffer>("buffer_component");

    QFileDialog fileDialog(this);
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    fileDialog.setFileMode(QFileDialog::AnyFile);

    QHash<QString, BufferExporter::OutputType> outputExtensions;
    outputExtensions[tr("Image File (*.png)")] = BufferExporter::OutputType::Bitmap;
    outputExtensions[tr("Octave Raw Matrix (*.oct)")] = BufferExporter::OutputType::OctaveMatrix;

    QHashIterator<QString, BufferExporter::OutputType> it(outputExtensions);
    QString saveMessage;
    while (it.hasNext())
    {
      it.next();
      saveMessage += it.key();
      if (it.hasNext())
        saveMessage += ";;";
    }

    fileDialog.setNameFilter(saveMessage);

    if (fileDialog.exec() == QDialog::Accepted) {
      string fileName = fileDialog.selectedFiles()[0].toStdString();

      BufferExporter::export_buffer(component, fileName, outputExtensions[fileDialog.selectedNameFilter()]);
    }
}

void MainWindow::set_plot_callback(int (*plot_cbk)(const char *)) {
    plot_callback_ = plot_cbk;
}

void MainWindow::resizeEvent(QResizeEvent*) {
    persist_settings();
}

void MainWindow::moveEvent(QMoveEvent*) {
    persist_settings();
}

void MainWindow::closeEvent(QCloseEvent*) {
    is_window_ready_ = false;
    persist_settings();
}

deque<string> MainWindow::get_observed_symbols() {
    deque<string> observed_names;

    for(const auto& name: held_buffers_) {
        observed_names.push_back(name.first);
    }

    return observed_names;
}

bool MainWindow::is_window_ready() {
    return is_window_ready_;
}

void MainWindow::show_context_menu(const QPoint& pos)
{
    // Handle global position
    QPoint globalPos = ui_->imageList->mapToGlobal(pos);

    // Create menu and insert context actions
    QMenu myMenu(this);

    QAction *exportAction = myMenu.addAction("Export buffer", this, SLOT(export_buffer()));
    // Add parameter to action: buffer name
    exportAction->setData(ui_->imageList->itemAt(pos)->data(Qt::UserRole));

    // Show context menu at handling position
    myMenu.exec(globalPos);
}
