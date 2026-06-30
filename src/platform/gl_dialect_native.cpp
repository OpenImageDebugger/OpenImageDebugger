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

#include "platform/gl_dialect.h"

namespace oid {

GLuint GlDialect::texture_internal_format(GLenum /*tex_type*/,
                                          GLenum /*tex_format*/) const
{
    return GL_RGBA32F;
}

const GlDialect& the_dialect() {
    static const GlDialect dialect{
        .version_directive        = "#version 120\n",
        .fragment_preamble        = "",
        .uses_out_color           = false,
        .icon_image_format        = QImage::Format_RGB888,
        .icon_bytes_per_pixel     = 3,
        .has_texture_wrap_r       = true,
        .icon_gl_internal_format  = GL_RGB8,
        .icon_gl_format           = GL_RGB,
    };
    return dialect;
}

} // namespace oid
