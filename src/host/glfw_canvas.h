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

#ifndef HOST_GLFW_CANVAS_H_
#define HOST_GLFW_CANVAS_H_

#include <functional>
#include <memory>
#include <utility>

#include <GL/gl.h>

struct GLFWwindow;

namespace oid {
class GLTextRenderer;
class Stage;
} // namespace oid

namespace oid::host {

// Qt-free canvas the shared visualization layer renders through in the
// GLFW/ImGui build. Dispatches GL via pointers loaded from glfwGetProcAddress
// and reports size from the GLFW framebuffer.
class GlfwCanvas {
  public:
    // Returns the (width, height) that render_width()/render_height()
    // should report. Callers (ImGui frontend main()) pass a lambda reading
    // the current canvas-PANE size in LOGICAL points (the same units fed to
    // Stage::resize_callback() and set_mouse_position() each frame) -- NOT
    // the whole window's framebuffer size -- matching the native Qt canvas,
    // whose render_width() returned the widget's device-independent width
    // (the legacy Qt GLCanvas; see tag legacy-qt). Every consumer of
    // render_width()/ render_height() (the status bar's pixel unprojection,
    // Camera's zoom-anchor NDC math / initial projection / zoom limits, the
    // thumbnail icon render's post-render camera restore) operates in that
    // pane-logical frame. With no provider (size_override == nullptr),
    // render_width()/render_height() fall back to the window's GLFW
    // framebuffer size.
    using SizeProvider = std::function<std::pair<int, int>()>;

    explicit GlfwCanvas(GLFWwindow* window,
                        SizeProvider size_override = nullptr);
    // Out-of-line where struct Fns is complete so unique_ptr can destroy the
    // incomplete type. Declaring the destructor makes the class move-only and
    // non-copyable (the unique_ptr member deletes the copy operations), which
    // is what we want: a GlfwCanvas owns a single GL entry-point table.
    ~GlfwCanvas();

    bool load(); // resolve GL entry points; call once with the context current

    [[nodiscard]] int render_width() const;
    [[nodiscard]] int render_height() const;
    [[nodiscard]] bool is_ready() const {
        return ready_;
    }
    // Lazily bakes (once GL is ready) and returns the shared GLTextRenderer
    // used to draw pixel-value overlays, or nullptr if GL isn't loaded yet
    // or the embedded font failed to bake. The GL dispatch surface below is
    // const-callable, so the only mutable state this needs is
    // `text_renderer_` and `text_renderer_init_attempted_` for the
    // one-attempt lazy-init side effect.
    [[nodiscard]] const oid::GLTextRenderer* get_text_renderer() const;

    // Host-tracked mouse position (device pixels); fed by the ImGui input
    // bridge before each scroll_callback so the viz layer's camera can
    // anchor zoom at the live cursor position. Defaults to the origin so the
    // camera scroll math has a well-defined value to read before the first
    // update.
    [[nodiscard]] int mouse_x() const {
        return mouse_x_;
    }
    [[nodiscard]] int mouse_y() const {
        return mouse_y_;
    }
    void set_mouse_position(int x, int y) {
        mouse_x_ = x;
        mouse_y_ = y;
    }

    // --- GL dispatch surface used by the viz layer (exact set below) ---
    GLuint glCreateShader(GLenum type) const;
    void glShaderSource(GLuint s,
                        GLsizei n,
                        const GLchar* const* str,
                        const GLint* len) const;
    void glCompileShader(GLuint s) const;
    void glGetShaderiv(GLuint s, GLenum p, GLint* v) const;
    void glGetShaderInfoLog(GLuint s, GLsizei b, GLsizei* l, GLchar* log) const;
    GLuint glCreateProgram() const;
    void glAttachShader(GLuint p, GLuint s) const;
    void glLinkProgram(GLuint p) const;
    void glGetProgramiv(GLuint p, GLenum pn, GLint* v) const;
    void
    glGetProgramInfoLog(GLuint p, GLsizei b, GLsizei* l, GLchar* log) const;
    void glDeleteShader(GLuint s) const;
    void glDeleteProgram(GLuint p) const;
    void glUseProgram(GLuint p) const;
    GLint glGetUniformLocation(GLuint p, const GLchar* n) const;
    void glUniform1i(GLint l, GLint v) const;
    void glUniform2f(GLint l, GLfloat a, GLfloat b) const;
    void glUniform3fv(GLint l, GLsizei n, const GLfloat* v) const;
    void glUniform4fv(GLint l, GLsizei n, const GLfloat* v) const;
    void
    glUniformMatrix4fv(GLint l, GLsizei n, GLboolean t, const GLfloat* v) const;
    void glGenBuffers(GLsizei n, GLuint* b) const;
    void glDeleteBuffers(GLsizei n, const GLuint* b) const;
    void glBindBuffer(GLenum t, GLuint b) const;
    void glBufferData(GLenum t, GLsizeiptr sz, const void* d, GLenum u) const;
    void glEnableVertexAttribArray(GLuint i) const;
    void glVertexAttribPointer(GLuint i,
                               GLint s,
                               GLenum t,
                               GLboolean n,
                               GLsizei st,
                               const void* p) const;
    void glGenTextures(GLsizei n, GLuint* t) const;
    void glDeleteTextures(GLsizei n, const GLuint* t) const;
    void glBindTexture(GLenum tg, GLuint t) const;
    void glActiveTexture(GLenum tex) const;
    void glTexParameteri(GLenum tg, GLenum p, GLint v) const;
    void glTexImage2D(GLenum tg,
                      GLint lv,
                      GLint ifmt,
                      GLsizei w,
                      GLsizei h,
                      GLint b,
                      GLenum f,
                      GLenum ty,
                      const void* d) const;
    void glTexSubImage2D(GLenum tg,
                         GLint lv,
                         GLint xo,
                         GLint yo,
                         GLsizei w,
                         GLsizei h,
                         GLenum f,
                         GLenum ty,
                         const void* d) const;
    void glGenerateMipmap(GLenum tg) const;
    void glPixelStorei(GLenum p, GLint v) const;
    void glDrawArrays(GLenum m, GLint f, GLsizei c) const;

    // --- FBO + frame-clear surface used by host-side render-to-texture
    // views (e.g. StageView); not needed by the viz layer itself, which
    // never binds a framebuffer or clears the screen on its own. ---
    void glGenFramebuffers(GLsizei n, GLuint* f) const;
    void glBindFramebuffer(GLenum target, GLuint f) const;
    void glFramebufferTexture2D(GLenum target,
                                GLenum attachment,
                                GLenum textarget,
                                GLuint texture,
                                GLint level) const;
    GLenum glCheckFramebufferStatus(GLenum target) const;
    void glDeleteFramebuffers(GLsizei n, const GLuint* f) const;
    // A core-profile GL context (which macOS forces for GL >= 3.2) draws
    // nothing unless a vertex array object is bound; the viz layer relies on
    // one being present (under Qt it came from QOpenGLWidget's default VAO).
    void glGenVertexArrays(GLsizei n, GLuint* a) const;
    void glBindVertexArray(GLuint a) const;
    void glDeleteVertexArrays(GLsizei n, const GLuint* a) const;
    void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) const;
    void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) const;
    void glClear(GLbitfield mask) const;
    // Alpha blending, needed by the pixel-value text overlay (its glyph
    // coverage is written to the fragment alpha). Set per render pass rather
    // than once, since ImGui's GL backend saves/restores blend state around
    // its own draw and can leave it disabled between our passes.
    void glEnable(GLenum cap) const;
    void glBlendFunc(GLenum sfactor, GLenum dfactor) const;
    void glDisable(GLenum cap) const;
    void glReadPixels(GLint x,
                      GLint y,
                      GLsizei w,
                      GLsizei h,
                      GLenum format,
                      GLenum type,
                      void* data) const;

    // Buffer-list thumbnail icon rendering: renders `stage` into a small
    // offscreen FBO and reads the result back into stage.get_buffer_icon(),
    // mirroring GLCanvas::render_buffer_icon / init_icon_framebuffer 1:1.
    // init_icon_framebuffer() lazily allocates the icon FBO + color texture
    // on first use; ~GlfwCanvas() tears both down. render_buffer_icon() is
    // defined in glfw_canvas_icon.cpp to keep this TU's link graph viz-free
    // (see the note atop that file).
    bool init_icon_framebuffer(int icon_width, int icon_height);
    bool render_buffer_icon(oid::Stage& stage, int icon_width, int icon_height);

  private:
    void destroy_icon_framebuffer();

    GLFWwindow* window_;
    SizeProvider size_override_;
    bool ready_{false};
    int mouse_x_{0};
    int mouse_y_{0};
    struct Fns; // opaque function-pointer table
    std::unique_ptr<Fns> fns_;
    // Baked lazily on first get_text_renderer() call once GL is ready; see
    // that method for the one-attempt gating.
    mutable std::unique_ptr<oid::GLTextRenderer> text_renderer_;
    mutable bool text_renderer_init_attempted_{false};
    // Icon FBO + color texture backing render_buffer_icon(); lazily created
    // by init_icon_framebuffer(), torn down in ~GlfwCanvas().
    GLuint icon_fbo_{0};
    GLuint icon_texture_{0};
    // The core GL profile requires a bound VAO for any draw call. StageView
    // binds its own VAO for the on-screen pass; render_buffer_icon() runs its
    // stage.draw() outside that pass and needs one too, or nothing rasterizes
    // and the icon comes out blank.
    GLuint icon_vao_{0};
    bool icon_framebuffer_ready_{false};
};

} // namespace oid::host

#endif // HOST_GLFW_CANVAS_H_
