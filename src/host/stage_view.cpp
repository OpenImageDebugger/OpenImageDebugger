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

#include "host/stage_view.h"

#include "host/glfw_canvas.h"
#include "visualization/stage.h"

namespace oid::host {

StageView::StageView(GlfwCanvas& canvas) : canvas_(canvas) {
    canvas_.glGenVertexArrays(1, &vao_);
}

StageView::~StageView() {
    destroy();
    if (vao_ != 0) {
        canvas_.glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
}

void StageView::destroy() {
    if (fbo_ != 0) {
        canvas_.glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    if (texture_ != 0) {
        canvas_.glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    width_ = 0;
    height_ = 0;
}

void StageView::ensure_size(const int width, const int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    if (!stage_view_needs_realloc(width_, height_, width, height)) {
        return;
    }

    destroy();

    canvas_.glGenTextures(1, &texture_);
    canvas_.glBindTexture(GL_TEXTURE_2D, texture_);
    canvas_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    canvas_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    canvas_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    canvas_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    canvas_.glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA8,
                         width,
                         height,
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         nullptr);

    canvas_.glGenFramebuffers(1, &fbo_);
    canvas_.glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    canvas_.glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);

    if (canvas_.glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE) [[unlikely]] {
        canvas_.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        destroy();
        return;
    }

    canvas_.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    width_ = width;
    height_ = height;
}

GLuint StageView::render(Stage& stage) const {
    if (fbo_ == 0) {
        return 0;
    }

    canvas_.glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    canvas_.glViewport(0, 0, width_, height_);
    canvas_.glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    canvas_.glClear(GL_COLOR_BUFFER_BIT);

    // A VAO must be bound for the viz layer's glDrawArrays calls to produce
    // output in a core-profile context; bind ours each frame since ImGui's
    // backend rebinds/clears the VAO binding when it renders.
    // Enable alpha blending for this pass so the pixel-value text overlay
    // (glyph coverage lives in the fragment alpha) composites over the buffer
    // instead of painting opaque glyph-quad boxes. Set every frame: ImGui's GL
    // backend saves/restores blend state around its own render and leaves it
    // disabled between our passes (same reason the VAO is rebound above).
    canvas_.glEnable(GL_BLEND);
    canvas_.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    canvas_.glBindVertexArray(vao_);
    stage.update();
    stage.draw();
    canvas_.glBindVertexArray(0);

    canvas_.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return texture_;
}

} // namespace oid::host
