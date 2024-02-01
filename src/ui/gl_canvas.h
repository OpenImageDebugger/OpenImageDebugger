/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2024 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
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

#ifndef GL_CANVAS_H_
#define GL_CANVAS_H_

#include <memory>

#include <QMouseEvent>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>


class MainWindow;
class Stage;
class GLTextRenderer;


class GLCanvas final : public QOpenGLWidget, public QOpenGLFunctions
{
    Q_OBJECT
  public:
    explicit GLCanvas(QWidget* parent = nullptr);

    ~GLCanvas() override;

    void mouseMoveEvent(QMouseEvent* ev) override;

    void mousePressEvent(QMouseEvent* ev) override;

    void mouseReleaseEvent(QMouseEvent* ev) override;

    void initializeGL() override;

    void paintGL() override;

    void resizeGL(int w, int h) override;

    void wheelEvent(QWheelEvent* ev) override;

    [[nodiscard]] int mouse_x() const
    {
        return mouse_x_;
    }

    [[nodiscard]] int mouse_y() const
    {
        return mouse_y_;
    }

    [[nodiscard]] bool is_mouse_down() const
    {
        return mouse_down_[0];
    }

    [[nodiscard]] bool is_ready() const
    {
        return initialized_;
    }

    [[nodiscard]] const GLTextRenderer* get_text_renderer() const;

    void set_main_window(MainWindow* mw);

    void render_buffer_icon(Stage* stage, int icon_width, int icon_height);

  private:
    bool mouse_down_[2]{};

    int mouse_x_;
    int mouse_y_;

    MainWindow* main_window_;

    GLuint icon_texture_;
    GLuint icon_fbo_;

    bool initialized_;

    std::unique_ptr<GLTextRenderer> text_renderer_;

    void generate_icon_texture();
};

#endif // GL_CANVAS_H_
