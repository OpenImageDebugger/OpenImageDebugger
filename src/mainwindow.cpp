#include <sstream>

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    currently_selected_stage_(nullptr),
    ui(new Ui::MainWindow),
    ac_enabled(true)
{
    ui->setupUi(this);

    connect(&update_timer_, SIGNAL(timeout()), this, SLOT(loop()));
    connect(ui->imageList, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(buffer_selected(QListWidgetItem*)));
    ui->bufferPreview->set_main_window(this);

    // Configure auto contrast inputs
    ui->ac_red_min->setValidator(   new QDoubleValidator() );
    ui->ac_green_min->setValidator( new QDoubleValidator() );
    ui->ac_blue_min->setValidator(  new QDoubleValidator() );

    ui->ac_red_max->setValidator(   new QDoubleValidator() );
    ui->ac_green_max->setValidator( new QDoubleValidator() );
    ui->ac_blue_max->setValidator(  new QDoubleValidator() );

    connect(ui->ac_red_min, SIGNAL(editingFinished()), this, SLOT(ac_red_min_update()));
    connect(ui->ac_red_max, SIGNAL(editingFinished()), this, SLOT(ac_red_max_update()));
    connect(ui->ac_green_min, SIGNAL(editingFinished()), this, SLOT(ac_green_min_update()));
    connect(ui->ac_green_max, SIGNAL(editingFinished()), this, SLOT(ac_green_max_update()));
    connect(ui->ac_blue_min, SIGNAL(editingFinished()), this, SLOT(ac_blue_min_update()));
    connect(ui->ac_blue_max, SIGNAL(editingFinished()), this, SLOT(ac_blue_max_update()));

    connect(ui->ac_reset_min, SIGNAL(clicked()), this, SLOT(ac_min_reset()));
    connect(ui->ac_reset_max, SIGNAL(clicked()), this, SLOT(ac_max_reset()));

    connect(ui->acToggle, SIGNAL(clicked()), this, SLOT(ac_toggle()));
}

MainWindow::~MainWindow()
{
    for(auto& held_buffer: held_buffers_)
        Py_DECREF(held_buffer.second);

    delete ui;
}

void MainWindow::draw()
{
    if(currently_selected_stage_ != nullptr) {
        currently_selected_stage_->draw();
    }
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

    ui->ac_red_min->setText(QString::number(ac_min[0]));

    if(buffer->channels == 3) {
        if(!ui->ac_green_min->isEnabled()) {
            ui->ac_green_min->setEnabled(true);
            ui->ac_blue_min->setEnabled(true);
        }
        ui->ac_green_min->setText(QString::number(ac_min[1]));
        ui->ac_blue_min->setText(QString::number(ac_min[2]));
    } else {
        ui->ac_green_min->setEnabled(false);
        ui->ac_blue_min->setEnabled(false);
    }
}

void MainWindow::reset_ac_max_labels()
{
    Buffer* buffer = currently_selected_stage_->getComponent<Buffer>("buffer_component");
    float* ac_max = buffer->max_buffer_values();

    ui->ac_red_max->setText(QString::number(ac_max[0]));
    if(buffer->channels == 3) {
        if(!ui->ac_green_max->isEnabled()) {
            ui->ac_green_max->setEnabled(true);
            ui->ac_blue_max->setEnabled(true);
        }
        ui->ac_green_max->setText(QString::number(ac_max[1]));
        ui->ac_blue_max->setText(QString::number(ac_max[2]));
    } else {
        ui->ac_green_max->setEnabled(false);
        ui->ac_blue_max->setEnabled(false);
    }
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
            if(!stage->initialize(ui->bufferPreview, buffer, request.width_i, request.height_i,
                                  request.channels, request.type, request.step, ac_enabled)) {
                cerr << "[error] Could not initialize opengl canvas!"<<endl;
            }
            stages_[request.var_name_str] = stage;

            int bytes_per_line = request.step * request.channels;
            QImage bufferIcon(buffer, request.width_i, request.height_i, bytes_per_line,
                              QImage::Format_RGB888);
            if(bufferIcon.width() > bufferIcon.height())
                bufferIcon = bufferIcon.scaledToWidth(200);
            else
                bufferIcon = bufferIcon.scaledToHeight(100);
            bufferIcon.save("/tmp/teste.png");

            stringstream label;
            label << request.var_name_str << "\n[" << request.width_i << "x" <<
                     request.height_i << "]\nuint8x3";
            QListWidgetItem* item = new QListWidgetItem(QPixmap::fromImage(bufferIcon),
                                                        label.str().c_str());
            item->setData(Qt::UserRole, QString(request.var_name_str.c_str()));
            item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
            item->setSizeHint(QSize(205,200));
            item->setTextAlignment(Qt::AlignHCenter);
            ui->imageList->addItem(item);
        } else {
            Py_DECREF(held_buffers_[request.var_name_str]);
            held_buffers_[request.var_name_str] = request.py_buffer;
            buffer_stage->second->buffer_update(buffer, request.width_i, request.height_i,
                                                request.channels, request.type,
                                                request.step);
            // Update AC values
            if(currently_selected_stage_ != nullptr) {
                reset_ac_min_labels();
                reset_ac_max_labels();
            }
        }

        pending_updates_.pop_front();
    }

    ui->bufferPreview->updateGL();
    if(currently_selected_stage_ != nullptr) {
        currently_selected_stage_->update();
    }
}

void MainWindow::buffer_selected(QListWidgetItem * item) {
    std::map<std::string, std::shared_ptr<Stage>>::iterator stage = stages_.find(item->data(Qt::UserRole).toString().toStdString());
    if(stage != stages_.end()) {
        currently_selected_stage_ = stage->second.get();
        reset_ac_min_labels();
        reset_ac_max_labels();
    }
}

void MainWindow::ac_red_min_update()
{
    set_ac_min_value(0, ui->ac_red_min->text().toFloat());
}

void MainWindow::ac_green_min_update()
{
    set_ac_min_value(1, ui->ac_green_min->text().toFloat());
}

void MainWindow::ac_blue_min_update()
{
    set_ac_min_value(2, ui->ac_blue_min->text().toFloat());
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

void MainWindow::ac_red_max_update()
{
    set_ac_max_value(0, ui->ac_red_max->text().toFloat());
}

void MainWindow::ac_green_max_update()
{
    set_ac_max_value(1, ui->ac_green_max->text().toFloat());
}

void MainWindow::ac_blue_max_update()
{
    set_ac_max_value(2, ui->ac_blue_max->text().toFloat());
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
    ac_enabled = !ac_enabled;
    for(auto& stage: stages_)
        stage.second->contrast_enabled = ac_enabled;
}
