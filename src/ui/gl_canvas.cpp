/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 OpenImageDebugger contributors
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

#include "gl_canvas.h"

#include "main_window/main_window.h"
#include "ui/gl_text_renderer.h"
#include "visualization/components/camera.h"
#include "visualization/game_object.h"


using namespace std;


GLCanvas::GLCanvas(QWidget* parent)
    : QOpenGLWidget(parent)
    , QOpenGLFunctions()
    , mouse_x_(0)
    , mouse_y_(0)
    , initialized_(false)
    , text_renderer_(new GLTextRenderer(this))
{
    mouse_down_[0] = mouse_down_[1] = false;
}


GLCanvas::~GLCanvas()
{
}


void GLCanvas::mouseMoveEvent(QMouseEvent* ev)
{
    int last_mouse_x = mouse_x_;
    int last_mouse_y = mouse_y_;

    mouse_x_ = ev->localPos().x();
    mouse_y_ = ev->localPos().y();

    if (mouse_down_[0]) {
        main_window_->mouse_drag_event(mouse_x_ - last_mouse_x,
                                       mouse_y_ - last_mouse_y);
    } else {
        main_window_->mouse_move_event(mouse_x_ - last_mouse_x,
                                       mouse_y_ - last_mouse_y);
    }
}


void GLCanvas::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton)
        mouse_down_[0] = true;

    if (ev->button() == Qt::RightButton)
        mouse_down_[1] = true;
}


void GLCanvas::mouseReleaseEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton)
        mouse_down_[0] = false;

    if (ev->button() == Qt::RightButton)
        mouse_down_[1] = false;
}


void GLCanvas::initializeGL()
{
    this->makeCurrent();
    initializeOpenGLFunctions();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    ///
    // Texture for generating icons
    assert(main_window_ != nullptr);
    QSizeF icon_size = main_window_->get_icon_size();
    int icon_width   = icon_size.width();
    int icon_height  = icon_size.height();
    glGenTextures(1, &icon_texture_);
    glBindTexture(GL_TEXTURE_2D, icon_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB8,
                 icon_width,
                 icon_height,
                 0,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 NULL);

    // Generate FBO
    glGenFramebuffers(1, &icon_fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, icon_fbo_);

    // Attach 2D texture to this FBO
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, icon_texture_, 0);

    // Check if the GPU won't freak out about our FBO
    GLenum status;
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    switch (status) {
    case GL_FRAMEBUFFER_COMPLETE:
        break;
    default:
        cerr << "Error: FBO configuration is not supported -- sorry mate!"
             << endl;
        break;
    }

    glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);

    // Initialize text renderer
    text_renderer_->initialize();

    initialized_ = true;
}


void GLCanvas::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    main_window_->draw();
}


void GLCanvas::wheelEvent(QWheelEvent* ev)
{
    main_window_->scroll_callback(ev->delta() / 120.0f);
}


const GLTextRenderer* GLCanvas::get_text_renderer()
{
    return text_renderer_.get();
}


void GLCanvas::render_buffer_icon(Stage* stage, int icon_width, int icon_height)
{
    glBindFramebuffer(GL_FRAMEBUFFER_EXT, icon_fbo_);

    glViewport(0, 0, icon_width, icon_height);

    GameObject* camera = stage->get_game_object("camera");
    Camera* cam        = camera->get_component<Camera>("camera_component");

    // Save original camera pose
    Camera original_pose = *cam;

    // Adapt camera to the thumbnail dimentions
    cam->window_resized(icon_width, icon_height);
    // Flips the projected image along the horizontal axis
    cam->projection.set_ortho_projection(
        icon_width / 2.0, -icon_height / 2.0, -1.0f, 1.0f);
    // Reposition buffer in the center of the canvas
    cam->recenter_camera();

    stage->draw();
    stage->buffer_icon.resize(3 * icon_width * icon_height);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0,
                 0,
                 icon_width,
                 icon_height,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 stage->buffer_icon.data());

    // Reset stage camera
    glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
    glViewport(0, 0, width(), height());
    *cam = original_pose;
    cam->window_resized(width(), height());
}


void GLCanvas::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    main_window_->resize_callback(w, h);
}


void GLCanvas::set_main_window(MainWindow* mw)
{
    main_window_ = mw;
}


void list_gl_extensions()
{
    GLint num_exts = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_exts);

    cout << "Supported OpenGL extensions:" << glGetString(GL_EXTENSIONS)
         << endl;
}
