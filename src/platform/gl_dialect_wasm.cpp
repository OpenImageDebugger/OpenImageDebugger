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

GLuint GlDialect::texture_internal_format(GLenum tex_type,
                                          GLenum tex_format) const
{
    if (tex_type == GL_FLOAT) {
        if (tex_format == GL_RED) {
            return GL_R32F;
        }
        if (tex_format == GL_RG) {
            return GL_RG32F;
        }
        if (tex_format == GL_RGB) {
            return GL_RGB32F;
        }
        return GL_RGBA32F;
    }
    if (tex_format == GL_RED) {
        return GL_R8;
    }
    if (tex_format == GL_RG) {
        return GL_RG8;
    }
    if (tex_format == GL_RGB) {
        return GL_RGB8;
    }
    return GL_RGBA8;
}

const GlDialect& the_dialect() {
    static const GlDialect dialect{
        .version_directive        = "#version 300 es\n",
        .fragment_preamble        = "precision mediump float;\n"
                                    "precision mediump int;\n"
                                    "out vec4 oid_fragColor;\n",
        .uses_out_color           = true,
        .icon_image_format        = QImage::Format_RGBA8888,
        .icon_bytes_per_pixel     = 4,
        .has_texture_wrap_r       = false,
        .icon_gl_internal_format  = GL_RGBA8,
        .icon_gl_format           = GL_RGBA,
    };
    return dialect;
}

} // namespace oid
