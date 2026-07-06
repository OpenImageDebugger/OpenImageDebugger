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

// GlfwCanvas::render_buffer_icon() lives in its own translation unit,
// separate from glfw_canvas.cpp, purely for build-graph reasons: it is the
// only part of GlfwCanvas that touches the shared visualization layer
// (Stage/GameObject/Camera), and keeping it out of glfw_canvas.cpp lets
// glfw_canvas_test (a pure-logic test: size math + null text renderer via
// SizeProvider injection) keep compiling/linking just glfw_canvas.cpp
// without dragging in Stage/Camera/GameObject/Component and their
// transitive dependencies (Eigen, the rest of VIZ_SOURCES). This file is
// only added to IMGUI_FRONTEND_SOURCES (see src/CMakeLists.txt), which
// already links the full VIZ_SOURCES set, so oidwindow is unaffected.
//
// Ported 1:1 from GLCanvas::render_buffer_icon in the legacy Qt frontend's
// GL canvas (see tag legacy-qt), including the camera save/restore and the
// FBO/viewport/icon drawing mode restoration sequence.

#include "host/glfw_canvas.h"

#include "platform/gl_dialect.h"
#include "visualization/components/camera.h"
// Required for GameObject::get_component<>() template method instantiation
// (mirrors the same include note in the legacy Qt GLCanvas; see tag legacy-qt).
#include "visualization/game_object.h"
#include "visualization/stage.h"

namespace oid::host {

bool GlfwCanvas::render_buffer_icon(oid::Stage& stage,
                                    const int icon_width,
                                    const int icon_height) {
    if (!ready_) {
        return false;
    }
    // Lazily allocate the icon FBO/texture on first use (same shape as
    // get_text_renderer()'s lazy bake), rather than eagerly at construction:
    // most frames never render a thumbnail (e.g. an empty buffer list), so
    // this avoids allocating GL resources that may never be needed.
    if (!icon_framebuffer_ready_ &&
        !init_icon_framebuffer(icon_width, icon_height)) {
        return false;
    }

    const auto& dialect = oid::the_dialect();
    const auto icon_read_format = dialect.icon_gl_format;
    const auto icon_bytes_per_pixel = dialect.icon_bytes_per_pixel;

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
        camera->get().get_component<oid::Camera>("camera_component");
    if (!cam_opt.has_value()) [[unlikely]] {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, render_width(), render_height());
        return false;
    }
    auto& cam = cam_opt->get();

    // Save original camera pose
    const auto original_pose = oid::Camera{cam};

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

    // A VAO must be bound for draws in the core GL profile (StageView binds its
    // own for the on-screen pass; this offscreen pass needs one too, or nothing
    // rasterizes and the icon is blank).
    glBindVertexArray(icon_vao_);
    stage.draw();
    glBindVertexArray(0);
    stage.get_buffer_icon().resize(static_cast<size_t>(icon_bytes_per_pixel) *
                                   static_cast<size_t>(icon_width) *
                                   static_cast<size_t>(icon_height));
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

} // namespace oid::host
