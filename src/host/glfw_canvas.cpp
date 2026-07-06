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

#include "host/glfw_canvas.h"

// macOS's system OpenGL headers (pulled in transitively by glfw3.h) mark the
// desktop GL entry points used below as deprecated in favor of Metal. This
// project still targets desktop GL (see src/platform/gl_dialect.h), so we
// silence the deprecation warning rather than treat it as a -Werror failure.
// Same pattern used by Dear ImGui's own GLFW+GL examples/backends.
#define GL_SILENCE_DEPRECATION

#include <GLFW/glfw3.h>

#include <iostream>

#include "host/text/stb_glyph_atlas.h"
#include "platform/gl_dialect.h"
#include "visualization/gl_text_renderer.h"

namespace oid::host {

struct GlfwCanvas::Fns {
    using PFN_glCreateShader = GLuint (*)(GLenum);
    using PFN_glShaderSource = void (*)(GLuint,
                                        GLsizei,
                                        const GLchar* const*,
                                        const GLint*);
    using PFN_glCompileShader = void (*)(GLuint);
    using PFN_glGetShaderiv = void (*)(GLuint, GLenum, GLint*);
    using PFN_glGetShaderInfoLog = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
    using PFN_glCreateProgram = GLuint (*)();
    using PFN_glAttachShader = void (*)(GLuint, GLuint);
    using PFN_glLinkProgram = void (*)(GLuint);
    using PFN_glGetProgramiv = void (*)(GLuint, GLenum, GLint*);
    using PFN_glGetProgramInfoLog = void (*)(GLuint,
                                             GLsizei,
                                             GLsizei*,
                                             GLchar*);
    using PFN_glDeleteShader = void (*)(GLuint);
    using PFN_glDeleteProgram = void (*)(GLuint);
    using PFN_glUseProgram = void (*)(GLuint);
    using PFN_glGetUniformLocation = GLint (*)(GLuint, const GLchar*);
    using PFN_glUniform1i = void (*)(GLint, GLint);
    using PFN_glUniform2f = void (*)(GLint, GLfloat, GLfloat);
    using PFN_glUniform3fv = void (*)(GLint, GLsizei, const GLfloat*);
    using PFN_glUniform4fv = void (*)(GLint, GLsizei, const GLfloat*);
    using PFN_glUniformMatrix4fv = void (*)(GLint,
                                            GLsizei,
                                            GLboolean,
                                            const GLfloat*);
    using PFN_glGenBuffers = void (*)(GLsizei, GLuint*);
    using PFN_glDeleteBuffers = void (*)(GLsizei, const GLuint*);
    using PFN_glBindBuffer = void (*)(GLenum, GLuint);
    using PFN_glBufferData = void (*)(GLenum, GLsizeiptr, const void*, GLenum);
    using PFN_glEnableVertexAttribArray = void (*)(GLuint);
    using PFN_glVertexAttribPointer =
        void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
    using PFN_glGenTextures = void (*)(GLsizei, GLuint*);
    using PFN_glDeleteTextures = void (*)(GLsizei, const GLuint*);
    using PFN_glBindTexture = void (*)(GLenum, GLuint);
    using PFN_glActiveTexture = void (*)(GLenum);
    using PFN_glTexParameteri = void (*)(GLenum, GLenum, GLint);
    using PFN_glTexImage2D = void (*)(GLenum,
                                      GLint,
                                      GLint,
                                      GLsizei,
                                      GLsizei,
                                      GLint,
                                      GLenum,
                                      GLenum,
                                      const void*);
    using PFN_glTexSubImage2D = void (*)(GLenum,
                                         GLint,
                                         GLint,
                                         GLint,
                                         GLsizei,
                                         GLsizei,
                                         GLenum,
                                         GLenum,
                                         const void*);
    using PFN_glGenerateMipmap = void (*)(GLenum);
    using PFN_glPixelStorei = void (*)(GLenum, GLint);
    using PFN_glDrawArrays = void (*)(GLenum, GLint, GLsizei);
    using PFN_glGenFramebuffers = void (*)(GLsizei, GLuint*);
    using PFN_glBindFramebuffer = void (*)(GLenum, GLuint);
    using PFN_glFramebufferTexture2D =
        void (*)(GLenum, GLenum, GLenum, GLuint, GLint);
    using PFN_glCheckFramebufferStatus = GLenum (*)(GLenum);
    using PFN_glDeleteFramebuffers = void (*)(GLsizei, const GLuint*);
    using PFN_glGenVertexArrays = void (*)(GLsizei, GLuint*);
    using PFN_glBindVertexArray = void (*)(GLuint);
    using PFN_glDeleteVertexArrays = void (*)(GLsizei, const GLuint*);
    using PFN_glViewport = void (*)(GLint, GLint, GLsizei, GLsizei);
    using PFN_glClearColor = void (*)(GLfloat, GLfloat, GLfloat, GLfloat);
    using PFN_glClear = void (*)(GLbitfield);
    using PFN_glEnable = void (*)(GLenum);
    using PFN_glBlendFunc = void (*)(GLenum, GLenum);
    using PFN_glDisable = void (*)(GLenum);
    using PFN_glReadPixels =
        void (*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);

    PFN_glCreateShader pfn_glCreateShader{nullptr};
    PFN_glShaderSource pfn_glShaderSource{nullptr};
    PFN_glCompileShader pfn_glCompileShader{nullptr};
    PFN_glGetShaderiv pfn_glGetShaderiv{nullptr};
    PFN_glGetShaderInfoLog pfn_glGetShaderInfoLog{nullptr};
    PFN_glCreateProgram pfn_glCreateProgram{nullptr};
    PFN_glAttachShader pfn_glAttachShader{nullptr};
    PFN_glLinkProgram pfn_glLinkProgram{nullptr};
    PFN_glGetProgramiv pfn_glGetProgramiv{nullptr};
    PFN_glGetProgramInfoLog pfn_glGetProgramInfoLog{nullptr};
    PFN_glDeleteShader pfn_glDeleteShader{nullptr};
    PFN_glDeleteProgram pfn_glDeleteProgram{nullptr};
    PFN_glUseProgram pfn_glUseProgram{nullptr};
    PFN_glGetUniformLocation pfn_glGetUniformLocation{nullptr};
    PFN_glUniform1i pfn_glUniform1i{nullptr};
    PFN_glUniform2f pfn_glUniform2f{nullptr};
    PFN_glUniform3fv pfn_glUniform3fv{nullptr};
    PFN_glUniform4fv pfn_glUniform4fv{nullptr};
    PFN_glUniformMatrix4fv pfn_glUniformMatrix4fv{nullptr};
    PFN_glGenBuffers pfn_glGenBuffers{nullptr};
    PFN_glDeleteBuffers pfn_glDeleteBuffers{nullptr};
    PFN_glBindBuffer pfn_glBindBuffer{nullptr};
    PFN_glBufferData pfn_glBufferData{nullptr};
    PFN_glEnableVertexAttribArray pfn_glEnableVertexAttribArray{nullptr};
    PFN_glVertexAttribPointer pfn_glVertexAttribPointer{nullptr};
    PFN_glGenTextures pfn_glGenTextures{nullptr};
    PFN_glDeleteTextures pfn_glDeleteTextures{nullptr};
    PFN_glBindTexture pfn_glBindTexture{nullptr};
    PFN_glActiveTexture pfn_glActiveTexture{nullptr};
    PFN_glTexParameteri pfn_glTexParameteri{nullptr};
    PFN_glTexImage2D pfn_glTexImage2D{nullptr};
    PFN_glTexSubImage2D pfn_glTexSubImage2D{nullptr};
    PFN_glGenerateMipmap pfn_glGenerateMipmap{nullptr};
    PFN_glPixelStorei pfn_glPixelStorei{nullptr};
    PFN_glDrawArrays pfn_glDrawArrays{nullptr};
    PFN_glGenFramebuffers pfn_glGenFramebuffers{nullptr};
    PFN_glBindFramebuffer pfn_glBindFramebuffer{nullptr};
    PFN_glFramebufferTexture2D pfn_glFramebufferTexture2D{nullptr};
    PFN_glCheckFramebufferStatus pfn_glCheckFramebufferStatus{nullptr};
    PFN_glDeleteFramebuffers pfn_glDeleteFramebuffers{nullptr};
    PFN_glGenVertexArrays pfn_glGenVertexArrays{nullptr};
    PFN_glBindVertexArray pfn_glBindVertexArray{nullptr};
    PFN_glDeleteVertexArrays pfn_glDeleteVertexArrays{nullptr};
    PFN_glViewport pfn_glViewport{nullptr};
    PFN_glClearColor pfn_glClearColor{nullptr};
    PFN_glClear pfn_glClear{nullptr};
    PFN_glEnable pfn_glEnable{nullptr};
    PFN_glBlendFunc pfn_glBlendFunc{nullptr};
    PFN_glDisable pfn_glDisable{nullptr};
    PFN_glReadPixels pfn_glReadPixels{nullptr};
};

GlfwCanvas::GlfwCanvas(GLFWwindow* window, SizeProvider size_override)
    : window_(window), size_override_(std::move(size_override)),
      fns_(std::make_unique<Fns>()) {}

// Defined here (not defaulted in the header) so struct Fns is complete at the
// point unique_ptr<Fns> instantiates its deleter. Also tears down the icon
// FBO/texture (a no-op if render_buffer_icon() was never called): both are
// only ever allocated after load() succeeded, so the GL entry points needed
// to delete them are still valid here.
GlfwCanvas::~GlfwCanvas() {
    destroy_icon_framebuffer();
}

namespace {

// glfwGetProcAddress returns a generic function pointer; converting it to
// the concrete GL signature requires exactly one reinterpret_cast, which
// lives here so the load() body stays cast-free.
template <typename PFN> PFN load_gl(const char* name) {
    return reinterpret_cast<PFN>(glfwGetProcAddress(name));
}

} // namespace

bool GlfwCanvas::load() {
    // Requires a current GL context (glfwMakeContextCurrent). Only
    // meaningfully callable at runtime once a window exists; unit tests
    // exercise render_width/height() and get_text_renderer() without it.
    fns_->pfn_glCreateShader =
        load_gl<Fns::PFN_glCreateShader>("glCreateShader");
    fns_->pfn_glShaderSource =
        load_gl<Fns::PFN_glShaderSource>("glShaderSource");
    fns_->pfn_glCompileShader =
        load_gl<Fns::PFN_glCompileShader>("glCompileShader");
    fns_->pfn_glGetShaderiv = load_gl<Fns::PFN_glGetShaderiv>("glGetShaderiv");
    fns_->pfn_glGetShaderInfoLog =
        load_gl<Fns::PFN_glGetShaderInfoLog>("glGetShaderInfoLog");
    fns_->pfn_glCreateProgram =
        load_gl<Fns::PFN_glCreateProgram>("glCreateProgram");
    fns_->pfn_glAttachShader =
        load_gl<Fns::PFN_glAttachShader>("glAttachShader");
    fns_->pfn_glLinkProgram = load_gl<Fns::PFN_glLinkProgram>("glLinkProgram");
    fns_->pfn_glGetProgramiv =
        load_gl<Fns::PFN_glGetProgramiv>("glGetProgramiv");
    fns_->pfn_glGetProgramInfoLog =
        load_gl<Fns::PFN_glGetProgramInfoLog>("glGetProgramInfoLog");
    fns_->pfn_glDeleteShader =
        load_gl<Fns::PFN_glDeleteShader>("glDeleteShader");
    fns_->pfn_glDeleteProgram =
        load_gl<Fns::PFN_glDeleteProgram>("glDeleteProgram");
    fns_->pfn_glUseProgram = load_gl<Fns::PFN_glUseProgram>("glUseProgram");
    fns_->pfn_glGetUniformLocation =
        load_gl<Fns::PFN_glGetUniformLocation>("glGetUniformLocation");
    fns_->pfn_glUniform1i = load_gl<Fns::PFN_glUniform1i>("glUniform1i");
    fns_->pfn_glUniform2f = load_gl<Fns::PFN_glUniform2f>("glUniform2f");
    fns_->pfn_glUniform3fv = load_gl<Fns::PFN_glUniform3fv>("glUniform3fv");
    fns_->pfn_glUniform4fv = load_gl<Fns::PFN_glUniform4fv>("glUniform4fv");
    fns_->pfn_glUniformMatrix4fv =
        load_gl<Fns::PFN_glUniformMatrix4fv>("glUniformMatrix4fv");
    fns_->pfn_glGenBuffers = load_gl<Fns::PFN_glGenBuffers>("glGenBuffers");
    fns_->pfn_glDeleteBuffers =
        load_gl<Fns::PFN_glDeleteBuffers>("glDeleteBuffers");
    fns_->pfn_glBindBuffer = load_gl<Fns::PFN_glBindBuffer>("glBindBuffer");
    fns_->pfn_glBufferData = load_gl<Fns::PFN_glBufferData>("glBufferData");
    fns_->pfn_glEnableVertexAttribArray =
        load_gl<Fns::PFN_glEnableVertexAttribArray>(
            "glEnableVertexAttribArray");
    fns_->pfn_glVertexAttribPointer =
        load_gl<Fns::PFN_glVertexAttribPointer>("glVertexAttribPointer");
    fns_->pfn_glGenTextures = load_gl<Fns::PFN_glGenTextures>("glGenTextures");
    fns_->pfn_glDeleteTextures =
        load_gl<Fns::PFN_glDeleteTextures>("glDeleteTextures");
    fns_->pfn_glBindTexture = load_gl<Fns::PFN_glBindTexture>("glBindTexture");
    fns_->pfn_glActiveTexture =
        load_gl<Fns::PFN_glActiveTexture>("glActiveTexture");
    fns_->pfn_glTexParameteri =
        load_gl<Fns::PFN_glTexParameteri>("glTexParameteri");
    fns_->pfn_glTexImage2D = load_gl<Fns::PFN_glTexImage2D>("glTexImage2D");
    fns_->pfn_glTexSubImage2D =
        load_gl<Fns::PFN_glTexSubImage2D>("glTexSubImage2D");
    fns_->pfn_glGenerateMipmap =
        load_gl<Fns::PFN_glGenerateMipmap>("glGenerateMipmap");
    fns_->pfn_glPixelStorei = load_gl<Fns::PFN_glPixelStorei>("glPixelStorei");
    fns_->pfn_glDrawArrays = load_gl<Fns::PFN_glDrawArrays>("glDrawArrays");
    fns_->pfn_glGenFramebuffers =
        load_gl<Fns::PFN_glGenFramebuffers>("glGenFramebuffers");
    fns_->pfn_glBindFramebuffer =
        load_gl<Fns::PFN_glBindFramebuffer>("glBindFramebuffer");
    fns_->pfn_glFramebufferTexture2D =
        load_gl<Fns::PFN_glFramebufferTexture2D>("glFramebufferTexture2D");
    fns_->pfn_glCheckFramebufferStatus =
        load_gl<Fns::PFN_glCheckFramebufferStatus>("glCheckFramebufferStatus");
    fns_->pfn_glDeleteFramebuffers =
        load_gl<Fns::PFN_glDeleteFramebuffers>("glDeleteFramebuffers");
    fns_->pfn_glGenVertexArrays =
        load_gl<Fns::PFN_glGenVertexArrays>("glGenVertexArrays");
    fns_->pfn_glBindVertexArray =
        load_gl<Fns::PFN_glBindVertexArray>("glBindVertexArray");
    fns_->pfn_glDeleteVertexArrays =
        load_gl<Fns::PFN_glDeleteVertexArrays>("glDeleteVertexArrays");
    fns_->pfn_glViewport = load_gl<Fns::PFN_glViewport>("glViewport");
    fns_->pfn_glClearColor = load_gl<Fns::PFN_glClearColor>("glClearColor");
    fns_->pfn_glClear = load_gl<Fns::PFN_glClear>("glClear");
    fns_->pfn_glEnable = load_gl<Fns::PFN_glEnable>("glEnable");
    fns_->pfn_glBlendFunc = load_gl<Fns::PFN_glBlendFunc>("glBlendFunc");
    fns_->pfn_glDisable = load_gl<Fns::PFN_glDisable>("glDisable");
    fns_->pfn_glReadPixels = load_gl<Fns::PFN_glReadPixels>("glReadPixels");

    ready_ = fns_->pfn_glCreateShader != nullptr &&
             fns_->pfn_glShaderSource != nullptr &&
             fns_->pfn_glCompileShader != nullptr &&
             fns_->pfn_glGetShaderiv != nullptr &&
             fns_->pfn_glGetShaderInfoLog != nullptr &&
             fns_->pfn_glCreateProgram != nullptr &&
             fns_->pfn_glAttachShader != nullptr &&
             fns_->pfn_glLinkProgram != nullptr &&
             fns_->pfn_glGetProgramiv != nullptr &&
             fns_->pfn_glGetProgramInfoLog != nullptr &&
             fns_->pfn_glDeleteShader != nullptr &&
             fns_->pfn_glDeleteProgram != nullptr &&
             fns_->pfn_glUseProgram != nullptr &&
             fns_->pfn_glGetUniformLocation != nullptr &&
             fns_->pfn_glUniform1i != nullptr &&
             fns_->pfn_glUniform2f != nullptr &&
             fns_->pfn_glUniform3fv != nullptr &&
             fns_->pfn_glUniform4fv != nullptr &&
             fns_->pfn_glUniformMatrix4fv != nullptr &&
             fns_->pfn_glGenBuffers != nullptr &&
             fns_->pfn_glDeleteBuffers != nullptr &&
             fns_->pfn_glBindBuffer != nullptr &&
             fns_->pfn_glBufferData != nullptr &&
             fns_->pfn_glEnableVertexAttribArray != nullptr &&
             fns_->pfn_glVertexAttribPointer != nullptr &&
             fns_->pfn_glGenTextures != nullptr &&
             fns_->pfn_glDeleteTextures != nullptr &&
             fns_->pfn_glBindTexture != nullptr &&
             fns_->pfn_glActiveTexture != nullptr &&
             fns_->pfn_glTexParameteri != nullptr &&
             fns_->pfn_glTexImage2D != nullptr &&
             fns_->pfn_glTexSubImage2D != nullptr &&
             fns_->pfn_glGenerateMipmap != nullptr &&
             fns_->pfn_glPixelStorei != nullptr &&
             fns_->pfn_glDrawArrays != nullptr &&
             fns_->pfn_glGenFramebuffers != nullptr &&
             fns_->pfn_glBindFramebuffer != nullptr &&
             fns_->pfn_glFramebufferTexture2D != nullptr &&
             fns_->pfn_glCheckFramebufferStatus != nullptr &&
             fns_->pfn_glDeleteFramebuffers != nullptr &&
             fns_->pfn_glGenVertexArrays != nullptr &&
             fns_->pfn_glBindVertexArray != nullptr &&
             fns_->pfn_glDeleteVertexArrays != nullptr &&
             fns_->pfn_glViewport != nullptr &&
             fns_->pfn_glClearColor != nullptr && fns_->pfn_glClear != nullptr;

    return ready_;
}

int GlfwCanvas::render_width() const {
    if (size_override_) {
        return size_override_().first;
    }
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    return width;
}

int GlfwCanvas::render_height() const {
    if (size_override_) {
        return size_override_().second;
    }
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    return height;
}

GLuint GlfwCanvas::glCreateShader(GLenum type) const {
    return fns_->pfn_glCreateShader(type);
}

void GlfwCanvas::glShaderSource(GLuint s,
                                GLsizei n,
                                const GLchar* const* str,
                                const GLint* len) const {
    fns_->pfn_glShaderSource(s, n, str, len);
}

void GlfwCanvas::glCompileShader(GLuint s) const {
    fns_->pfn_glCompileShader(s);
}

void GlfwCanvas::glGetShaderiv(GLuint s, GLenum p, GLint* v) const {
    fns_->pfn_glGetShaderiv(s, p, v);
}

void GlfwCanvas::glGetShaderInfoLog(GLuint s,
                                    GLsizei b,
                                    GLsizei* l,
                                    GLchar* log) const {
    fns_->pfn_glGetShaderInfoLog(s, b, l, log);
}

GLuint GlfwCanvas::glCreateProgram() const {
    return fns_->pfn_glCreateProgram();
}

void GlfwCanvas::glAttachShader(GLuint p, GLuint s) const {
    fns_->pfn_glAttachShader(p, s);
}

void GlfwCanvas::glLinkProgram(GLuint p) const {
    fns_->pfn_glLinkProgram(p);
}

void GlfwCanvas::glGetProgramiv(GLuint p, GLenum pn, GLint* v) const {
    fns_->pfn_glGetProgramiv(p, pn, v);
}

void GlfwCanvas::glGetProgramInfoLog(GLuint p,
                                     GLsizei b,
                                     GLsizei* l,
                                     GLchar* log) const {
    fns_->pfn_glGetProgramInfoLog(p, b, l, log);
}

void GlfwCanvas::glDeleteShader(GLuint s) const {
    fns_->pfn_glDeleteShader(s);
}

void GlfwCanvas::glDeleteProgram(GLuint p) const {
    fns_->pfn_glDeleteProgram(p);
}

void GlfwCanvas::glUseProgram(GLuint p) const {
    fns_->pfn_glUseProgram(p);
}

GLint GlfwCanvas::glGetUniformLocation(GLuint p, const GLchar* n) const {
    return fns_->pfn_glGetUniformLocation(p, n);
}

void GlfwCanvas::glUniform1i(GLint l, GLint v) const {
    fns_->pfn_glUniform1i(l, v);
}

void GlfwCanvas::glUniform2f(GLint l, GLfloat a, GLfloat b) const {
    fns_->pfn_glUniform2f(l, a, b);
}

void GlfwCanvas::glUniform3fv(GLint l, GLsizei n, const GLfloat* v) const {
    fns_->pfn_glUniform3fv(l, n, v);
}

void GlfwCanvas::glUniform4fv(GLint l, GLsizei n, const GLfloat* v) const {
    fns_->pfn_glUniform4fv(l, n, v);
}

void GlfwCanvas::glUniformMatrix4fv(GLint l,
                                    GLsizei n,
                                    GLboolean t,
                                    const GLfloat* v) const {
    fns_->pfn_glUniformMatrix4fv(l, n, t, v);
}

void GlfwCanvas::glGenBuffers(GLsizei n, GLuint* b) const {
    fns_->pfn_glGenBuffers(n, b);
}

void GlfwCanvas::glDeleteBuffers(GLsizei n, const GLuint* b) const {
    fns_->pfn_glDeleteBuffers(n, b);
}

void GlfwCanvas::glBindBuffer(GLenum t, GLuint b) const {
    fns_->pfn_glBindBuffer(t, b);
}

void GlfwCanvas::glBufferData(GLenum t,
                              GLsizeiptr sz,
                              const void* d,
                              GLenum u) const {
    fns_->pfn_glBufferData(t, sz, d, u);
}

void GlfwCanvas::glEnableVertexAttribArray(GLuint i) const {
    fns_->pfn_glEnableVertexAttribArray(i);
}

void GlfwCanvas::glVertexAttribPointer(
    GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void* p) const {
    fns_->pfn_glVertexAttribPointer(i, s, t, n, st, p);
}

void GlfwCanvas::glGenTextures(GLsizei n, GLuint* t) const {
    fns_->pfn_glGenTextures(n, t);
}

void GlfwCanvas::glDeleteTextures(GLsizei n, const GLuint* t) const {
    fns_->pfn_glDeleteTextures(n, t);
}

void GlfwCanvas::glBindTexture(GLenum tg, GLuint t) const {
    fns_->pfn_glBindTexture(tg, t);
}

void GlfwCanvas::glActiveTexture(GLenum tex) const {
    fns_->pfn_glActiveTexture(tex);
}

void GlfwCanvas::glTexParameteri(GLenum tg, GLenum p, GLint v) const {
    fns_->pfn_glTexParameteri(tg, p, v);
}

void GlfwCanvas::glTexImage2D(GLenum tg,
                              GLint lv,
                              GLint ifmt,
                              GLsizei w,
                              GLsizei h,
                              GLint b,
                              GLenum f,
                              GLenum ty,
                              const void* d) const {
    fns_->pfn_glTexImage2D(tg, lv, ifmt, w, h, b, f, ty, d);
}

void GlfwCanvas::glTexSubImage2D(GLenum tg,
                                 GLint lv,
                                 GLint xo,
                                 GLint yo,
                                 GLsizei w,
                                 GLsizei h,
                                 GLenum f,
                                 GLenum ty,
                                 const void* d) const {
    fns_->pfn_glTexSubImage2D(tg, lv, xo, yo, w, h, f, ty, d);
}

void GlfwCanvas::glGenerateMipmap(GLenum tg) const {
    fns_->pfn_glGenerateMipmap(tg);
}

void GlfwCanvas::glPixelStorei(GLenum p, GLint v) const {
    fns_->pfn_glPixelStorei(p, v);
}

void GlfwCanvas::glDrawArrays(GLenum m, GLint f, GLsizei c) const {
    fns_->pfn_glDrawArrays(m, f, c);
}

void GlfwCanvas::glGenFramebuffers(GLsizei n, GLuint* f) const {
    fns_->pfn_glGenFramebuffers(n, f);
}

void GlfwCanvas::glBindFramebuffer(GLenum target, GLuint f) const {
    fns_->pfn_glBindFramebuffer(target, f);
}

void GlfwCanvas::glFramebufferTexture2D(GLenum target,
                                        GLenum attachment,
                                        GLenum textarget,
                                        GLuint texture,
                                        GLint level) const {
    fns_->pfn_glFramebufferTexture2D(
        target, attachment, textarget, texture, level);
}

GLenum GlfwCanvas::glCheckFramebufferStatus(GLenum target) const {
    return fns_->pfn_glCheckFramebufferStatus(target);
}

void GlfwCanvas::glDeleteFramebuffers(GLsizei n, const GLuint* f) const {
    fns_->pfn_glDeleteFramebuffers(n, f);
}

void GlfwCanvas::glGenVertexArrays(GLsizei n, GLuint* a) const {
    fns_->pfn_glGenVertexArrays(n, a);
}

void GlfwCanvas::glBindVertexArray(GLuint a) const {
    fns_->pfn_glBindVertexArray(a);
}

void GlfwCanvas::glDeleteVertexArrays(GLsizei n, const GLuint* a) const {
    fns_->pfn_glDeleteVertexArrays(n, a);
}

void GlfwCanvas::glViewport(GLint x, GLint y, GLsizei w, GLsizei h) const {
    fns_->pfn_glViewport(x, y, w, h);
}

void GlfwCanvas::glClearColor(GLfloat r,
                              GLfloat g,
                              GLfloat b,
                              GLfloat a) const {
    fns_->pfn_glClearColor(r, g, b, a);
}

void GlfwCanvas::glClear(GLbitfield mask) const {
    fns_->pfn_glClear(mask);
}

void GlfwCanvas::glEnable(GLenum cap) const {
    fns_->pfn_glEnable(cap);
}

void GlfwCanvas::glBlendFunc(GLenum sfactor, GLenum dfactor) const {
    fns_->pfn_glBlendFunc(sfactor, dfactor);
}

void GlfwCanvas::glDisable(GLenum cap) const {
    fns_->pfn_glDisable(cap);
}

void GlfwCanvas::glReadPixels(GLint x,
                              GLint y,
                              GLsizei w,
                              GLsizei h,
                              GLenum format,
                              GLenum type,
                              void* data) const {
    fns_->pfn_glReadPixels(x, y, w, h, format, type, data);
}

bool GlfwCanvas::init_icon_framebuffer(int icon_width, int icon_height) {
    if (icon_framebuffer_ready_) {
        return true;
    }
    if (!ready_) {
        return false;
    }

    const auto& dialect = oid::the_dialect();
    const auto icon_internal_format = dialect.icon_gl_internal_format;
    const auto icon_format = dialect.icon_gl_format;

    glGenTextures(1, &icon_texture_);
    glBindTexture(GL_TEXTURE_2D, icon_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 static_cast<GLint>(icon_internal_format),
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
        std::cerr << "[Error] icon FBO configuration is not supported. Icon "
                     "framebuffer initialization failed."
                  << std::endl;
        destroy_icon_framebuffer();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Core-profile draws require a bound VAO; render_buffer_icon() binds this
    // around stage.draw() (see the member's comment in the header).
    glGenVertexArrays(1, &icon_vao_);

    icon_framebuffer_ready_ = true;
    return true;
}

void GlfwCanvas::destroy_icon_framebuffer() {
    if (icon_fbo_ != 0) {
        glDeleteFramebuffers(1, &icon_fbo_);
        icon_fbo_ = 0;
    }
    if (icon_texture_ != 0) {
        glDeleteTextures(1, &icon_texture_);
        icon_texture_ = 0;
    }
    if (icon_vao_ != 0) {
        glDeleteVertexArrays(1, &icon_vao_);
        icon_vao_ = 0;
    }
    icon_framebuffer_ready_ = false;
}

// GlfwCanvas::render_buffer_icon() is defined in glfw_canvas_icon.cpp, not
// here: it is the only member that touches the shared visualization layer,
// and keeping it out of this TU lets glfw_canvas_test link glfw_canvas.cpp
// without the viz dependency graph (see the note atop glfw_canvas_icon.cpp).
// init_icon_framebuffer()/destroy_icon_framebuffer() above stay here since
// they only touch plain GL state, not the viz layer.

const oid::GLTextRenderer* GlfwCanvas::get_text_renderer() const {
    if (!ready_) {
        return nullptr; // GL not loaded yet
    }
    if (!text_renderer_init_attempted_) {
        text_renderer_init_attempted_ = true; // one attempt, mutable members
        if (auto atlas = oid::host::stb_glyph_atlas()) {
            auto r =
                std::make_unique<oid::GLTextRenderer>(*this, std::move(*atlas));
            if (r->initialize()) {
                text_renderer_ = std::move(r);
            }
        }
    }
    return text_renderer_.get();
}

} // namespace oid::host
