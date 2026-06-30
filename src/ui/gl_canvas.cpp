/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
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

#include <cmath>
#include <iostream>

#include "main_window/main_window.h"
#include "ui/gl_text_renderer.h"
#include "visualization/components/camera.h"
// Required for GameObject::get_component<>() template method instantiation
#include "visualization/game_object.h"

namespace oid {

GLCanvas::GLCanvas(QWidget* parent)
    : GlWidgetBase{parent},
      text_renderer_{std::make_unique<GLTextRenderer>(*this)} {
    mouse_down_[0] = mouse_down_[1] = false;
    platform_ctor_init();
}

GLCanvas::~GLCanvas() = default;

void GLCanvas::mouseMoveEvent(QMouseEvent* ev) {
    const auto last_mouse_x = mouse_x_;
    const auto last_mouse_y = mouse_y_;

    const auto scale_x =
        static_cast<float>(render_width()) / static_cast<float>(qMax(1, width()));
    const auto scale_y = static_cast<float>(render_height()) /
                         static_cast<float>(qMax(1, height()));
    mouse_x_ = static_cast<int>(ev->position().x() * scale_x);
    mouse_y_ = static_cast<int>(ev->position().y() * scale_y);

    if (mouse_down_[0]) {
        main_window().mouse_drag_event(mouse_x_ - last_mouse_x,
                                       mouse_y_ - last_mouse_y);
    } else {
        main_window().mouse_move_event(mouse_x_ - last_mouse_x,
                                       mouse_y_ - last_mouse_y);
    }
}

void GLCanvas::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton) {
        mouse_down_[0] = true;
    }

    if (ev->button() == Qt::RightButton) {
        mouse_down_[1] = true;
    }
}

void GLCanvas::mouseReleaseEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton) {
        mouse_down_[0] = false;
    }

    if (ev->button() == Qt::RightButton) {
        mouse_down_[1] = false;
    }
}

void GLCanvas::init_icon_framebuffer() {
    if (icon_framebuffer_ready_) {
        return;
    }

    const auto icon_size = MainWindow::get_icon_size();
    const auto icon_width = static_cast<int>(icon_size.width());
    const auto icon_height = static_cast<int>(icon_size.height());

#ifdef __EMSCRIPTEN__
    constexpr auto icon_internal_format = GL_RGBA8;
    constexpr auto icon_format = GL_RGBA;
#else
    constexpr auto icon_internal_format = GL_RGB8;
    constexpr auto icon_format = GL_RGB;
#endif

    glGenTextures(1, &icon_texture_);
    glBindTexture(GL_TEXTURE_2D, icon_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 icon_internal_format,
                 icon_width,
                 icon_height,
                 0,
                 icon_format,
                 GL_UNSIGNED_BYTE,
                 nullptr);

    glGenFramebuffers(1, &icon_fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, icon_fbo_);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, icon_texture_, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        [[unlikely]] {
        std::cerr << "[Error] FBO configuration is not supported. Framebuffer "
                     "initialization failed."
                  << std::endl;
        destroy_icon_framebuffer();
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    icon_framebuffer_ready_ = true;
}

void GLCanvas::destroy_icon_framebuffer() {
    if (icon_fbo_ != 0) {
        glDeleteFramebuffers(1, &icon_fbo_);
        icon_fbo_ = 0;
    }
    if (icon_texture_ != 0) {
        glDeleteTextures(1, &icon_texture_);
        icon_texture_ = 0;
    }
    icon_framebuffer_ready_ = false;
}

void GLCanvas::wheelEvent(QWheelEvent* ev) {
    main_window().scroll_callback(static_cast<float>(ev->angleDelta().y()) /
                                  120.0f);
}

const GLTextRenderer* GLCanvas::get_text_renderer() const {
    return text_renderer_.get();
}

bool GLCanvas::render_buffer_icon(Stage& stage,
                                  const int icon_width,
                                  const int icon_height) {
    if (!initialized_ || !icon_framebuffer_ready_) {
        return false;
    }

#ifdef __EMSCRIPTEN__
    constexpr auto icon_read_format = GL_RGBA;
    constexpr int icon_bytes_per_pixel = 4;
#else
    constexpr auto icon_read_format = GL_RGB;
    constexpr int icon_bytes_per_pixel = 3;
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, icon_fbo_);

    glViewport(0, 0, icon_width, icon_height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const auto camera = stage.get_game_object("camera");
    if (!camera.has_value()) [[unlikely]] {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, render_width(), render_height());
        return false;
    }
    const auto cam_opt =
        camera->get().get_component<Camera>("camera_component");
    if (!cam_opt.has_value()) [[unlikely]] {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, render_width(), render_height());
        return false;
    }
    auto& cam = cam_opt->get();

    // Save original camera pose
    const auto original_pose = Camera{cam};

    // Adapt camera to the thumbnail dimensions
    cam.window_resized(icon_width, icon_height);
    // Flips the projected image along the horizontal axis
    cam.projection().set_ortho_projection(static_cast<float>(icon_width) / 2.0f,
                                          static_cast<float>(-icon_height) /
                                              2.0f,
                                          -1.0f,
                                          1.0f);
    // Reposition buffer in the center of the canvas
    cam.recenter_camera();

    // Enable icon drawing mode (forbids pixel borders drawing)
    stage.set_icon_drawing_mode(true);

    stage.draw();
    stage.get_buffer_icon().resize(
        static_cast<size_t>(icon_bytes_per_pixel) *
        static_cast<size_t>(icon_width) * static_cast<size_t>(icon_height));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glReadPixels(0,
                 0,
                 icon_width,
                 icon_height,
                 icon_read_format,
                 GL_UNSIGNED_BYTE,
                 stage.get_buffer_icon().data());

    // Reset stage camera
    stage.set_icon_drawing_mode(false);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, render_width(), render_height());
    cam = original_pose;
    cam.window_resized(render_width(), render_height());
    return true;
}

void GLCanvas::set_main_window(MainWindow& mw) {
    main_window_ = std::ref(mw);
}

void list_gl_extensions() {
    auto num_exts = GLint{0};
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_exts);

    std::cout << "Supported OpenGL extensions:" << glGetString(GL_EXTENSIONS)
              << std::endl;
}

} // namespace oid
