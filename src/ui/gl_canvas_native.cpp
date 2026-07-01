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

#include <iostream>

#include "main_window/main_window.h"
#include "ui/gl_text_renderer.h"

namespace oid {

int GLCanvas::render_width() const {
    return width();
}

int GLCanvas::render_height() const {
    return height();
}

void GLCanvas::initializeGL() {
    this->makeCurrent();
    if (const auto context = this->context();
        context == nullptr || !context->isValid()) [[unlikely]] {
        std::cerr << "[Error] OpenGL context is not valid. OpenGL "
                     "initialization cannot proceed."
                  << std::endl;
        initialized_ = false;
        return;
    }
    initializeOpenGLFunctions();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    init_icon_framebuffer();

    // Initialize text renderer
    if (!text_renderer_->initialize()) {
        initialized_ = false;
        return;
    }

    initialized_ = true;
}

void GLCanvas::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    main_window().draw();
}

void GLCanvas::resizeGL(const int w, const int h) {
    glViewport(0, 0, w, h);
    main_window().resize_callback(w, h);
}

void GLCanvas::platform_ctor_init() {}

} // namespace oid
