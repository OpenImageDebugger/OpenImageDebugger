#include <sstream>

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    currently_selected_stage_(nullptr)
{
    ui->setupUi(this);

    connect(&update_timer_, SIGNAL(timeout()), this, SLOT(loop()));
    connect(ui->imageList, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(buffer_selected(QListWidgetItem*)));
    ui->bufferPreview->set_main_window(this);
}

MainWindow::~MainWindow()
{
    delete ui;
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
        shared_ptr<Stage> stage = make_shared<Stage>();
        if(!stage->initialize(ui->bufferPreview, buffer, request.width_i, request.height_i,
                              request.channels, request.type, request.step)) {
            cerr << "[error] Could not initialize opengl canvas!"<<endl;
        }
        stages_[request.var_name_str] = stage;
        pending_updates_.pop_front();

        int bytes_per_line = request.width_i * request.channels;
        QImage bufferIcon(buffer, request.width_i, request.height_i, bytes_per_line,
                          QImage::Format_RGB888);
        if(bufferIcon.width() > bufferIcon.height())
            bufferIcon = bufferIcon.scaledToWidth(200);
        else
            bufferIcon = bufferIcon.scaledToHeight(100);

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
    }

    ui->bufferPreview->updateGL();
    if(currently_selected_stage_ != nullptr) {
        currently_selected_stage_->update();
        currently_selected_stage_->draw();
    }
}
