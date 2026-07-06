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

#ifndef HOST_STAGE_VIEW_H_
#define HOST_STAGE_VIEW_H_

#include <GL/gl.h>

namespace oid {
class Stage;
} // namespace oid

namespace oid::host {

class GlfwCanvas;

// Pure decision extracted out of StageView so it can be unit-tested without
// a live GL context: does a requested size differ from the currently
// allocated one, and therefore require the FBO + texture to be
// (re)allocated?
[[nodiscard]] constexpr bool
stage_view_needs_realloc(int cur_w, int cur_h, int req_w, int req_h) {
    return cur_w != req_w || cur_h != req_h;
}

// Renders a Stage into an off-screen FBO-backed RGBA8 texture, sized to
// whatever the caller requests each frame (typically the host window's
// framebuffer size). GL calls are dispatched through the GlfwCanvas& the
// Stage itself renders through, so there is a single GL entry-point table
// per window.
class StageView {
  public:
    explicit StageView(GlfwCanvas& canvas);
    ~StageView();

    StageView(const StageView&) = delete;
    StageView& operator=(const StageView&) = delete;
    StageView(StageView&&) = delete;
    StageView& operator=(StageView&&) = delete;

    // (Re)allocates the FBO + texture when the requested size changes;
    // no-op otherwise.
    void ensure_size(int width, int height);

    // Binds the FBO at the current size, sets the viewport, clears, runs
    // stage.update() + stage.draw(), rebinds the default framebuffer, and
    // returns the color texture id.
    [[nodiscard]] GLuint render(Stage& stage) const;

    [[nodiscard]] int width() const {
        return width_;
    }
    [[nodiscard]] int height() const {
        return height_;
    }

  private:
    void destroy();

    GlfwCanvas& canvas_;
    GLuint fbo_{0};
    GLuint texture_{0};
    // Core-profile GL requires a bound VAO for any draw; the viz layer does
    // not create one, so StageView owns one and keeps it bound while the
    // Stage renders. See glGenVertexArrays() note in glfw_canvas.h.
    GLuint vao_{0};
    int width_{0};
    int height_{0};
};

} // namespace oid::host

#endif // HOST_STAGE_VIEW_H_
