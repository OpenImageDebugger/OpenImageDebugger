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
#include "gl_text_renderer.h"

#include <algorithm>
#include <utility>

#include "platform/gl_dialect.h"
#include "visualization/shaders/oid_shaders.h"

namespace oid {

GLTextRenderer::GLTextRenderer(const RenderCanvas& canvas, GlyphAtlas atlas)
    : text_prog_{canvas}, canvas_{canvas}, atlas_{std::move(atlas)} {}

GLTextRenderer::~GLTextRenderer() {
    canvas_.glDeleteTextures(1, &text_tex_);
    canvas_.glDeleteBuffers(1, &text_vbo_);
}

bool GLTextRenderer::initialize() {
    if (!text_prog_.create(shader::text_vert_shader,
                           shader::text_frag_shader,
                           ShaderProgram::TexelChannels::FORMAT_R,
                           "rgba",
                           {"mvp",
                            "buff_sampler",
                            "text_sampler",
                            "pix_coord",
                            "brightness_contrast"})) {
        return false;
    }

    canvas_.glGenTextures(1, &text_tex_);
    canvas_.glActiveTexture(GL_TEXTURE0);
    canvas_.glBindTexture(GL_TEXTURE_2D, text_tex_);
    canvas_.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    canvas_.glGenBuffers(1, &text_vbo_);
    upload_atlas();

    return true;
}

void GLTextRenderer::upload_atlas() {
    canvas_.glActiveTexture(GL_TEXTURE0);
    canvas_.glBindTexture(GL_TEXTURE_2D, text_tex_);

    text_texture_width_ = atlas_.texture_width;
    text_texture_height_ = atlas_.texture_height;

    {
        constexpr auto mipmap_levels = 5;
        auto tex_level_width = static_cast<int>(text_texture_width_);
        auto tex_level_height = static_cast<int>(text_texture_height_);

        for (int i = 0; i < mipmap_levels; ++i) {
            canvas_.glTexImage2D(GL_TEXTURE_2D,
                                 i,
                                 GL_R8,
                                 tex_level_width,
                                 tex_level_height,
                                 0,
                                 GL_RED,
                                 GL_UNSIGNED_BYTE,
                                 nullptr);

            tex_level_width = (std::max)(1, tex_level_width / 2);
            tex_level_height = (std::max)(1, tex_level_height / 2);
        }
    }

    {
        canvas_.glTexSubImage2D(GL_TEXTURE_2D,
                                0,
                                0,
                                0,
                                static_cast<GLsizei>(text_texture_width_),
                                static_cast<GLsizei>(text_texture_height_),
                                GL_RED,
                                GL_UNSIGNED_BYTE,
                                atlas_.pixels.data());
    }

    text_texture_offsets_ = atlas_.offsets;
    text_texture_advances_ = atlas_.advances;
    text_texture_sizes_ = atlas_.sizes;
    text_texture_tls_ = atlas_.tls;

    canvas_.glGenerateMipmap(GL_TEXTURE_2D);
    canvas_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    canvas_.glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    const auto& dialect = the_dialect();
    const auto wrap_mode =
        dialect.has_texture_wrap_r ? GL_CLAMP_TO_BORDER : GL_CLAMP_TO_EDGE;
    canvas_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
    canvas_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);
    if (dialect.has_texture_wrap_r) {
        canvas_.glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    }
}

GLuint GLTextRenderer::text_vbo() const {
    return text_vbo_;
}

GLuint GLTextRenderer::text_tex() const {
    return text_tex_;
}

const ShaderProgram& GLTextRenderer::text_prog() const {
    return text_prog_;
}

const Array_256_2& GLTextRenderer::text_texture_offsets() const {
    return text_texture_offsets_;
}

const Array_256_2& GLTextRenderer::text_texture_advances() const {
    return text_texture_advances_;
}

const Array_256_2& GLTextRenderer::text_texture_sizes() const {
    return text_texture_sizes_;
}

const Array_256_2& GLTextRenderer::text_texture_tls() const {
    return text_texture_tls_;
}

float GLTextRenderer::text_texture_width() const {
    return text_texture_width_;
}

float GLTextRenderer::text_texture_height() const {
    return text_texture_height_;
}

} // namespace oid
