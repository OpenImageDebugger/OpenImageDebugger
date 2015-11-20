#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <Python.h>
#include <mutex>
#include <deque>
#include <string>
#include <QTimer>
#include <QListWidgetItem>

#include "glcanvas.hpp"
#include "stage.hpp"

namespace Ui {
class MainWindow;
}

struct BufferRequestMessage {
    std::string var_name_str;
    PyObject* py_buffer;
    int width_i;
    int height_i;
    int channels;
    int type;
    int step;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    void plot_buffer(BufferRequestMessage& buff);
    ~MainWindow();

    void show() {
        update_timer_.start(16);
        QMainWindow::show();
    }

    void resize_callback(int w, int h) {
        for(auto& stage: stages_)
            stage.second->resize_callback(w, h);
    }

    void scroll_callback(float delta) {
        if(currently_selected_stage_ != nullptr) {
            currently_selected_stage_->scroll_callback(delta);
        }
    }

public Q_SLOTS:
    void loop();
    void buffer_selected(QListWidgetItem * item);

    void ac_red_min_update();
    void ac_red_max_update();
    void ac_green_min_update();
    void ac_green_max_update();
    void ac_blue_min_update();
    void ac_blue_max_update();

    void ac_min_reset();
    void ac_max_reset();

    void ac_toggle();

private:
    QTimer update_timer_;
    Stage* currently_selected_stage_;
    std::map<std::string, std::shared_ptr<Stage>> stages_;
    std::mutex mtx_;
    std::deque<BufferRequestMessage> pending_updates_;
    Ui::MainWindow *ui;
    bool ac_enabled;

    QListWidgetItem* generateListItem(BufferRequestMessage&);
    void set_ac_min_value(int idx, float value);
    void set_ac_max_value(int idx, float value);
};

#endif // MAINWINDOW_H
