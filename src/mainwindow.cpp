#include <sstream>
#include <iomanip>

#include <QShortcut>

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    currently_selected_stage_(nullptr),
    completer_updated_(false),
    ui_(new Ui::MainWindow),
    ac_enabled_(true),
    link_views_enabled_(false),
    plot_callback(nullptr)
{
    ui_->setupUi(this);
    ui_->splitter->setSizes({210, 100000000});

    connect(&update_timer_, SIGNAL(timeout()), this, SLOT(loop()));

    symbol_list_focus_shortcut_ = shared_ptr<QShortcut>(new QShortcut(QKeySequence(Qt::CTRL|Qt::Key_K), this));
    connect(symbol_list_focus_shortcut_.get(), SIGNAL(activated()), ui_->symbolList, SLOT(setFocus()));

    connect(ui_->imageList, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), this, SLOT(buffer_selected(QListWidgetItem*)));
    buffer_removal_shortcut_ = shared_ptr<QShortcut>(new QShortcut(QKeySequence(Qt::Key_Delete), ui_->imageList));
    connect(buffer_removal_shortcut_.get(), SIGNAL(activated()), this, SLOT(remove_selected_buffer()));

    connect(ui_->symbolList, SIGNAL(editingFinished()), this, SLOT(on_symbol_selected()));

    ui_->bufferPreview->set_main_window(this);

    // Configure auto contrast inputs
    ui_->ac_red_min->setValidator(   new QDoubleValidator() );
    ui_->ac_green_min->setValidator( new QDoubleValidator() );
    ui_->ac_blue_min->setValidator(  new QDoubleValidator() );

    ui_->ac_red_max->setValidator(   new QDoubleValidator() );
    ui_->ac_green_max->setValidator( new QDoubleValidator() );
    ui_->ac_blue_max->setValidator(  new QDoubleValidator() );

    connect(ui_->ac_red_min, SIGNAL(editingFinished()), this, SLOT(ac_red_min_update()));
    connect(ui_->ac_red_max, SIGNAL(editingFinished()), this, SLOT(ac_red_max_update()));
    connect(ui_->ac_green_min, SIGNAL(editingFinished()), this, SLOT(ac_green_min_update()));
    connect(ui_->ac_green_max, SIGNAL(editingFinished()), this, SLOT(ac_green_max_update()));
    connect(ui_->ac_blue_min, SIGNAL(editingFinished()), this, SLOT(ac_blue_min_update()));
    connect(ui_->ac_blue_max, SIGNAL(editingFinished()), this, SLOT(ac_blue_max_update()));

    connect(ui_->ac_reset_min, SIGNAL(clicked()), this, SLOT(ac_min_reset()));
    connect(ui_->ac_reset_max, SIGNAL(clicked()), this, SLOT(ac_max_reset()));

    connect(ui_->acToggle, SIGNAL(clicked()), this, SLOT(ac_toggle()));

    connect(ui_->reposition_buffer, SIGNAL(clicked()), this, SLOT(recenter_buffer()));

    connect(ui_->linkViewsToggle, SIGNAL(clicked()), this, SLOT(link_views_toggle()));

    status_bar = new QLabel();
    status_bar->setAlignment(Qt::AlignRight);
    setStyleSheet("QStatusBar::item { border: 0px solid black };");
    statusBar()->addWidget(status_bar, 1);
}

MainWindow::~MainWindow()
{
    for(auto& held_buffer: held_buffers_)
        Py_DECREF(held_buffer.second);

    delete ui_;
}

void MainWindow::show() {
    update_timer_.start(16);
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
}

void MainWindow::get_observed_variables(PyObject *observed_set)
{
    for(const auto& stage: stages_) {
#if PY_MAJOR_VERSION >= 3
        PySet_Add(observed_set, PyUnicode_FromString(stage.first.c_str()));
#else
        PySet_Add(observed_set, PyString_FromString(stage.first.c_str()));
#endif
    }
}

void MainWindow::reset_ac_min_labels()
{
    Buffer* buffer = currently_selected_stage_->getComponent<Buffer>("buffer_component");
    float* ac_min = buffer->min_buffer_values();

    ui_->ac_red_min->setText(QString::number(ac_min[0]));

    if(buffer->channels == 3) {
        if(!ui_->ac_green_min->isEnabled()) {
            ui_->ac_green_min->setEnabled(true);
            ui_->ac_blue_min->setEnabled(true);
        }
        ui_->ac_green_min->setText(QString::number(ac_min[1]));
        ui_->ac_blue_min->setText(QString::number(ac_min[2]));
    } else {
        ui_->ac_green_min->setEnabled(false);
        ui_->ac_blue_min->setEnabled(false);
    }
}

void MainWindow::reset_ac_max_labels()
{
    Buffer* buffer = currently_selected_stage_->getComponent<Buffer>("buffer_component");
    float* ac_max = buffer->max_buffer_values();

    ui_->ac_red_max->setText(QString::number(ac_max[0]));
    if(buffer->channels == 3) {
        if(!ui_->ac_green_max->isEnabled()) {
            ui_->ac_green_max->setEnabled(true);
            ui_->ac_blue_max->setEnabled(true);
        }
        ui_->ac_green_max->setText(QString::number(ac_max[1]));
        ui_->ac_blue_max->setText(QString::number(ac_max[2]));
    } else {
        ui_->ac_green_max->setEnabled(false);
        ui_->ac_blue_max->setEnabled(false);
    }
}

void MainWindow::mouse_drag_event(int mouse_x, int mouse_y)
{
    if(link_views_enabled_) {
        for(auto& stage: stages_)
            stage.second->mouse_drag_event(mouse_x, mouse_y);
    } else if(currently_selected_stage_ != nullptr) {
        currently_selected_stage_->mouse_drag_event(mouse_x, mouse_y);
    }
}

void MainWindow::mouse_move_event(int, int)
{
    update_statusbar();
}

void MainWindow::plot_buffer(BufferRequestMessage &buff)
{
    BufferRequestMessage new_buffer;
    Py_INCREF(buff.py_buffer);
    new_buffer.var_name_str = buff.var_name_str;
    new_buffer.py_buffer = buff.py_buffer;
    new_buffer.width_i = buff.width_i;
    new_buffer.height_i = buff.height_i;
    new_buffer.channels = buff.channels;
    new_buffer.type = buff.type;
    new_buffer.step = buff.step;
    {
        std::unique_lock<std::mutex> lock(mtx_);
        pending_updates_.push_back(new_buffer);
    }

}

void MainWindow::loop() {
    while(!pending_updates_.empty()) {
        BufferRequestMessage request = pending_updates_.front();

        uint8_t* buffer = reinterpret_cast<uint8_t*>(
                    PyMemoryView_GET_BUFFER(request.py_buffer)->buf);
        auto buffer_stage = stages_.find(request.var_name_str);
        if(buffer_stage == stages_.end()) {
            // New buffer request
            held_buffers_[request.var_name_str] = request.py_buffer;
            shared_ptr<Stage> stage = make_shared<Stage>();
            if(!stage->initialize(ui_->bufferPreview, buffer, request.width_i, request.height_i,
                                  request.channels, request.type, request.step, ac_enabled_)) {
                cerr << "[error] Could not initialize opengl canvas!"<<endl;
            }
            stages_[request.var_name_str] = stage;

            QImage bufferIcon;
            ui_->bufferPreview->render_buffer_icon(stage.get());

            const int icon_width = 200;
            const int icon_height = 100;
            const int bytes_per_line = icon_width * 3;
            bufferIcon = QImage(stage->buffer_icon_.data(), icon_width,
                                icon_height, bytes_per_line, QImage::Format_RGB888);

            stringstream label;
            label << request.var_name_str << "\n[" << request.width_i << "x" <<
                     request.height_i << "]\nuint8x3";
            QListWidgetItem* item = new QListWidgetItem(QPixmap::fromImage(bufferIcon),
                                                        label.str().c_str());
            item->setData(Qt::UserRole, QString(request.var_name_str.c_str()));
            item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
            item->setSizeHint(QSize(205,bufferIcon.height() + 90));
            item->setTextAlignment(Qt::AlignHCenter);
            ui_->imageList->addItem(item);
        } else {
            Py_DECREF(held_buffers_[request.var_name_str]);
            held_buffers_[request.var_name_str] = request.py_buffer;
            buffer_stage->second->buffer_update(buffer, request.width_i, request.height_i,
                                                request.channels, request.type,
                                                request.step);
            // Update buffer icon
            Stage* stage = stages_[request.var_name_str].get();
            ui_->bufferPreview->render_buffer_icon(stage);

            // Looking for corresponding item...
            const int icon_width = 200;
            const int icon_height = 100;
            const int bytes_per_line = icon_width * 3;
            QImage bufferIcon(stage->buffer_icon_.data(), icon_width,
                                icon_height, bytes_per_line, QImage::Format_RGB888);
            stringstream label;
            label << request.var_name_str << "\n[" << request.width_i << "x" <<
                     request.height_i << "]\nuint8x3";

            for(int i = 0; i < ui_->imageList->count(); ++i) {
                QListWidgetItem* item = ui_->imageList->item(i);
                if(item->data(Qt::UserRole) == request.var_name_str.c_str()) {
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
    }

    if(completer_updated_) {
        if(ui_->symbolList->completer() != nullptr) {
            disconnect(ui_->symbolList->completer(), SIGNAL(activated(QString)),
                       this, SLOT(on_symbol_completed(QString)));
        }

        symbol_completer_ = shared_ptr<QCompleter>(new QCompleter(available_vars_));
        ui_->symbolList->setCompleter(symbol_completer_.get());

        connect(ui_->symbolList->completer(), SIGNAL(activated(QString)), this, SLOT(on_symbol_completed(QString)));

        completer_updated_ = false;
    }

    ui_->bufferPreview->updateGL();
    if(currently_selected_stage_ != nullptr) {
        currently_selected_stage_->update();
    }
}

void MainWindow::buffer_selected(QListWidgetItem * item) {
    if(item == nullptr)
        return;

    auto stage = stages_.find(item->data(Qt::UserRole).toString().toStdString());
    if(stage != stages_.end()) {
        currently_selected_stage_ = stage->second.get();
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

void MainWindow::set_ac_min_value(int idx, float value)
{
   if(currently_selected_stage_ != nullptr) {
       Buffer* buff = currently_selected_stage_->getComponent<Buffer>("buffer_component");
       buff->min_buffer_values()[idx] = value;
       buff->computeContrastBrightnessParameters();
   }
}

void MainWindow::set_ac_max_value(int idx, float value)
{
   if(currently_selected_stage_ != nullptr) {
       Buffer* buff = currently_selected_stage_->getComponent<Buffer>("buffer_component");
       buff->max_buffer_values()[idx] = value;
       buff->computeContrastBrightnessParameters();
   }
}

void MainWindow::update_statusbar()
{
    if(currently_selected_stage_ != nullptr) {
        stringstream message;
        Camera* cam = currently_selected_stage_->getComponent<Camera>("camera_component");

        float mouseX = ui_->bufferPreview->mouseX();
        float mouseY = ui_->bufferPreview->mouseY();
        float winW = ui_->bufferPreview->width();
        float winH = ui_->bufferPreview->height();
        vec4 mouse_pos_ndc( 2.0*(mouseX-winW/2)/winW, -2.0*(mouseY-winH/2)/winH, 0,1);
        mat4 view = cam->model.inv();
        mat4 vp_inv = (cam->projection*view).inv();

        vec4 mouse_pos = vp_inv * mouse_pos_ndc;

        message << std::fixed << std::setprecision(1) <<
                   "(" << floorf(mouse_pos.x) << "," << floorf(mouse_pos.y) << ")\t" <<
                   cam->zoom * 100.0 << "%";
        status_bar->setText(message.str().c_str());
    }
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

void MainWindow::ac_min_reset()
{
   if(currently_selected_stage_ != nullptr) {
       Buffer* buff = currently_selected_stage_->getComponent<Buffer>("buffer_component");
       buff->recomputeMinColorValues();
       buff->computeContrastBrightnessParameters();

       // Update inputs
       reset_ac_min_labels();
   }
}

void MainWindow::ac_max_reset()
{
   if(currently_selected_stage_ != nullptr) {
       Buffer* buff = currently_selected_stage_->getComponent<Buffer>("buffer_component");
       buff->recomputeMaxColorValues();
       buff->computeContrastBrightnessParameters();

       // Update inputs
       reset_ac_max_labels();
   }
}

void MainWindow::ac_toggle()
{
    ac_enabled_ = !ac_enabled_;
    for(auto& stage: stages_)
        stage.second->contrast_enabled = ac_enabled_;
}

void MainWindow::recenter_buffer()
{
   if(currently_selected_stage_ != nullptr) {
       Camera* cam = currently_selected_stage_->getComponent<Camera>("camera_component");
       cam->recenter_camera();
   }
}

void MainWindow::link_views_toggle()
{
    link_views_enabled_ = !link_views_enabled_;
}

void MainWindow::remove_selected_buffer()
{
    if(ui_->imageList->count() > 0 && currently_selected_stage_ != nullptr) {
        QListWidgetItem* removedItem = ui_->imageList->takeItem(ui_->imageList->currentRow());
        stages_.erase(removedItem->data(Qt::UserRole).toString().toStdString());

        if(stages_.size() == 0)
            currently_selected_stage_ = nullptr;
    }
}

void MainWindow::update_available_variables(PyObject *available_set)
{
    int count = PyList_Size(available_set);
    for(int i = 0; i < count; ++i) {
        PyObject *var_name_bytes = PyUnicode_AsEncodedString(PyList_GetItem(available_set, i), "ASCII", "strict");
        string var_name_str = PyBytes_AS_STRING(var_name_bytes);
        available_vars_.push_back(var_name_str.c_str());
    }

    completer_updated_ = true;
}

void MainWindow::on_symbol_selected() {
    const char* symbol_name = ui_->symbolList->text().toLocal8Bit().constData();
    plot_callback(symbol_name);
    // Clear symbol input
    ui_->symbolList->setText("");
}

void MainWindow::on_symbol_completed(QString str) {
    plot_callback(str.toLocal8Bit().constData());
    // Clear symbol input
    ui_->symbolList->setText("");
    ui_->symbolList->clearFocus();
}
