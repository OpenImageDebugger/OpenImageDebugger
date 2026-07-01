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

#include "ui/gl_canvas.h"

#include <cmath>
#include <rhi/qrhi.h>

#include "main_window/main_window.h"
#include "ui/gl_text_renderer.h"

namespace oid {

int GLCanvas::render_width() const {
    if (const auto* tex = colorTexture(); tex != nullptr) {
        return tex->pixelSize().width();
    }
    return static_cast<int>(std::lround(width() * devicePixelRatioF()));
}

int GLCanvas::render_height() const {
    if (const auto* tex = colorTexture(); tex != nullptr) {
        return tex->pixelSize().height();
    }
    return static_cast<int>(std::lround(height() * devicePixelRatioF()));
}

bool GLCanvas::init_gl_state() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    if (!text_renderer_->initialize()) {
        return false;
    }

    return true;
}

void GLCanvas::initialize(QRhiCommandBuffer* cb) {
    static QRhi* last_rhi = nullptr;
    if (last_rhi != rhi()) {
        initialized_ = false;
        first_paint_completed_ = false;
        last_rhi = rhi();
    }
    if (initialized_) {
        return;
    }

    const auto clear_color = QColor::fromRgbF(0.1f, 0.1f, 0.1f, 1.0f);
    cb->beginPass(renderTarget(),
                  clear_color,
                  {1.0f, 0},
                  nullptr,
                  QRhiCommandBuffer::ExternalContent);
    cb->beginExternal();
    initializeOpenGLFunctions();

    if (!init_gl_state()) {
        cb->endExternal();
        cb->endPass();
        return;
    }

    init_icon_framebuffer();

    cb->endExternal();
    cb->endPass();
    initialized_ = true;
}

void GLCanvas::render(QRhiCommandBuffer* cb) {
    if (!initialized_) {
        return;
    }

    const auto clear_color = QColor::fromRgbF(0.1f, 0.1f, 0.1f, 1.0f);
    cb->beginPass(renderTarget(),
                  clear_color,
                  {1.0f, 0},
                  nullptr,
                  QRhiCommandBuffer::ExternalContent);
    cb->beginExternal();

    drain_gl_queue();

    const auto w = render_width();
    const auto h = render_height();
    glViewport(0, 0, w, h);
    if (w != last_render_width_ || h != last_render_height_) {
        last_render_width_ = w;
        last_render_height_ = h;
        main_window().resize_callback(w, h);
    }
    main_window().prepare_gl_draw();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    main_window().draw();
    drain_icon_gl_queue();

    cb->endExternal();
    cb->endPass();

    first_paint_completed_ = true;
}

void GLCanvas::releaseResources() {
    initialized_ = false;
    first_paint_completed_ = false;
    last_render_width_ = 0;
    last_render_height_ = 0;
    destroy_icon_framebuffer();
}

void GLCanvas::resizeEvent(QResizeEvent* e) {
    QRhiWidget::resizeEvent(e);
    update();
}

void GLCanvas::schedule_gl(std::function<void()> task) {
    gl_queue_.push_back(std::move(task));
    update();
}

void GLCanvas::schedule_icon_gl(std::function<void()> task) {
    icon_gl_queue_.push_back(std::move(task));
    update();
}

void GLCanvas::drain_gl_queue() {
    while (!gl_queue_.empty()) {
        auto task = std::move(gl_queue_.front());
        gl_queue_.pop_front();
        task();
    }
}

void GLCanvas::drain_icon_gl_queue() {
    while (!icon_gl_queue_.empty()) {
        auto task = std::move(icon_gl_queue_.front());
        icon_gl_queue_.pop_front();
        task();
    }
}

void GLCanvas::platform_ctor_init() {
    setApi(Api::OpenGL);
    connect(this, &QRhiWidget::renderFailed, this, [this] {
        initialized_ = false;
        first_paint_completed_ = false;
    });
}

} // namespace oid
