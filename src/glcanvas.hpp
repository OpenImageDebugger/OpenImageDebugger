#pragma once

#include <memory>
#include <QGLWidget>
#include <QMouseEvent>

#include "buffer.hpp"
#include "camera.hpp"
#include "buffer_values.hpp"

class MainWindow;
class GLCanvas : public QGLWidget {
    Q_OBJECT
public:
    int mouseX() {
        return mouseX_;
    }

    int mouseY() {
        return mouseY_;
    }

    explicit GLCanvas(QWidget* parent = 0) : QGLWidget(parent) {
        mouseDown_[0] = mouseDown_[1] = false;
    }

    void mouseMoveEvent(QMouseEvent* ev) {
        mouseX_ = ev->localPos().x();
        mouseY_ = ev->localPos().y();
    }

    void mousePressEvent(QMouseEvent* ev) {
        if(ev->button() == Qt::LeftButton)
            mouseDown_[0] = true;
        if(ev->button() == Qt::RightButton)
            mouseDown_[1] = true;
    }

    void mouseReleaseEvent(QMouseEvent* ev) {
        if(ev->button() == Qt::LeftButton)
            mouseDown_[0] = false;
        if(ev->button() == Qt::RightButton)
            mouseDown_[1] = false;
    }

    bool isMouseDown() {
        return mouseDown_[0];
    }

    void initializeGL();

    void paintGL() {
        swapBuffers();
    }

    void resizeGL(int w, int h);

    void set_main_window(MainWindow* mw) {
        main_window_ = mw;
    }

    void wheelEvent(QWheelEvent* ev);

private:
    int mouseX_;
    int mouseY_;
    bool mouseDown_[2];
    MainWindow* main_window_;
};
