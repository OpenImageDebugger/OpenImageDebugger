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

#ifndef PLATFORM_GL_DIALECT_H_
#define PLATFORM_GL_DIALECT_H_

#include <string_view>

#include <GL/gl.h>
#include <QImage>

namespace oid {

struct GlDialect {
    std::string_view version_directive;
    std::string_view fragment_preamble;
    bool uses_out_color;
    QImage::Format icon_image_format;
    int icon_bytes_per_pixel;
    bool has_texture_wrap_r;
    GLenum icon_gl_internal_format;
    GLenum icon_gl_format;
    GLuint texture_internal_format(GLenum tex_type, GLenum tex_format) const;
};

const GlDialect& the_dialect();

} // namespace oid

#endif // PLATFORM_GL_DIALECT_H_
