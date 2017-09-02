#pragma once

#include <QGLWidget>
#include <QMouseEvent>

class MainWindow;
class Stage;

class GLCanvas : public QGLWidget {
    Q_OBJECT
public:
    int mouseX() {
        return mouseX_;
    }

    int mouseY() {
        return mouseY_;
    }

    explicit GLCanvas(QWidget* parent = 0);

    void mouseMoveEvent(QMouseEvent* ev);

    void mousePressEvent(QMouseEvent* ev);

    void mouseReleaseEvent(QMouseEvent* ev);

    bool isMouseDown() {
        return mouseDown_[0];
    }

    void initializeGL();

    void paintGL();

    void resizeGL(int w, int h);

    void set_main_window(MainWindow* mw);

    void render_buffer_icon(Stage *stage);

    void wheelEvent(QWheelEvent* ev);

private:
    int mouseX_;
    int mouseY_;
    bool mouseDown_[2];
    MainWindow* main_window_;
    GLuint icon_texture_;
    GLuint icon_fbo_;
};
