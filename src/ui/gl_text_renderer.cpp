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

#include <QPainter>
#include <QPixmap>

#include "visualization/shaders/oid_shaders.h"

namespace oid
{

GLTextRenderer::GLTextRenderer(GLCanvas& gl_canvas)
    : text_prog{gl_canvas}
    , gl_canvas_{gl_canvas}
{
}


GLTextRenderer::~GLTextRenderer()
{
    gl_canvas_.glDeleteTextures(1, &text_tex);
    gl_canvas_.glDeleteBuffers(1, &text_vbo);
}


bool GLTextRenderer::initialize()
{
    text_prog.create(shader::text_vert_shader,
                     shader::text_frag_shader,
                     ShaderProgram::TexelChannels::FormatR,
                     "rgba",
                     {"mvp",
                      "buff_sampler",
                      "text_sampler",
                      "pix_coord",
                      "brightness_contrast"});

    gl_canvas_.glGenTextures(1, &text_tex);
    gl_canvas_.glActiveTexture(GL_TEXTURE0);
    gl_canvas_.glBindTexture(GL_TEXTURE_2D, text_tex);
    gl_canvas_.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl_canvas_.glGenBuffers(1, &text_vbo);
    generate_glyphs_texture();

    return true;
}


void GLTextRenderer::generate_glyphs_texture()
{
    // Required characters for numbers, scientific notation (e), nan, inf
    constexpr auto text = "0123456789., -+enaif";
    const unsigned char* p{};
    constexpr auto border_size = 0;

    const auto g = QFontMetrics{font};

    gl_canvas_.glActiveTexture(GL_TEXTURE0);
    gl_canvas_.glBindTexture(GL_TEXTURE_2D, text_tex);

    // Generate text bitmap
    const auto texture_size = g.size(Qt::TextSingleLine, text);

    text_texture_width = text_texture_height = 1.0f;
    while (text_texture_width < static_cast<float>(texture_size.width())) {
        text_texture_width *= 2.0f;
    }
    while (text_texture_height < static_cast<float>(texture_size.height())) {
        text_texture_height *= 2.f;
    }

    {
        constexpr auto mipmap_levels = 5;
        auto tex_level_width         = static_cast<int>(text_texture_width);
        auto tex_level_height        = static_cast<int>(text_texture_height);

        for (int i = 0; i < mipmap_levels; ++i) {
            gl_canvas_.glTexImage2D(GL_TEXTURE_2D,
                                    i,
                                    GL_R8,
                                    tex_level_width,
                                    tex_level_height,
                                    0,
                                    GL_RED,
                                    GL_UNSIGNED_BYTE,
                                    nullptr);

            tex_level_width  = (std::max)(1, tex_level_width / 2);
            tex_level_height = (std::max)(1, tex_level_height / 2);
        }
    }

    auto pixmap = QPixmap{texture_size};
    pixmap.fill(QColor(0, 0, 0));
    auto painter = QPainter{&pixmap};
    painter.setPen(QColor(255, 255, 255));
    painter.setFont(font);
    painter.drawText(0, g.ascent(), text);
    const auto img =
        pixmap.toImage().convertToFormat(QImage::Format_Grayscale8);

    std::vector<uint8_t> packed_texture(
        static_cast<std::size_t>(text_texture_width * text_texture_height));
    auto packed_texture_ptr = packed_texture.data();

    auto real_ascent  = texture_size.height() - 1;
    auto real_descent = 0;

    auto found_real_descent = false;
    auto found_real_ascent  = false;
    // Pack bitmap and compute real ascent and descent lines
    for (int y = 0; y < texture_size.height(); ++y) {
        const auto img_ptr = img.scanLine(y);
        auto x             = 0;

        auto found_filled_pixel = false;
        for (; x < texture_size.width(); ++x) {
            packed_texture_ptr[x] = img_ptr[x];

            found_filled_pixel = found_filled_pixel || img_ptr[x] > 0;
        }

        // If the row was completely empty...
        if (!found_filled_pixel) {
            if (!found_real_descent) {
                real_descent = y;
            } else if (!found_real_ascent) {
                real_ascent       = y;
                found_real_ascent = true;
            }
        } else {
            found_real_descent = true;
        }

        for (; x < static_cast<int>(text_texture_width); ++x) {
            packed_texture_ptr[x] = 0;
        }
        packed_texture_ptr += static_cast<int>(text_texture_width);
    }


    // Compute text box size
    auto box_h = 0.0f;

    const auto cropped_bitmap_height = real_ascent - real_descent;

    for (p = reinterpret_cast<const unsigned char*>(text); *p; p++) {
        const auto advance_x     = g.horizontalAdvance(QChar(*p));
        const auto bitmap_height = g.height();

        text_texture_advances[*p][0] = advance_x;
        text_texture_advances[*p][1] = 0;
        text_texture_sizes[*p][0]    = advance_x;
        text_texture_sizes[*p][1]    = cropped_bitmap_height;
        text_texture_tls[*p][0]      = 0;
        text_texture_tls[*p][1]      = cropped_bitmap_height;

        box_h = (std::max)(box_h,
                           static_cast<float>(bitmap_height) + 2 * border_size);
    }

    // Clears generated buffer
    {
        gl_canvas_.glTexSubImage2D(GL_TEXTURE_2D,
                                   0,
                                   0,
                                   0,
                                   static_cast<GLsizei>(text_texture_width),
                                   static_cast<GLsizei>(text_texture_height),
                                   GL_RED,
                                   GL_UNSIGNED_BYTE,
                                   packed_texture.data());
    }

    auto x = 0;
    for (p = reinterpret_cast<const unsigned char*>(text); *p; p++) {
        const auto advance_x = g.horizontalAdvance(QChar(*p));

        text_texture_offsets[*p][0] = x + border_size;
        text_texture_offsets[*p][1] = real_descent + border_size;

        x += advance_x + border_size * 2;
    }

    gl_canvas_.glGenerateMipmap(GL_TEXTURE_2D);
    gl_canvas_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_canvas_.glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    gl_canvas_.glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    gl_canvas_.glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    gl_canvas_.glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
}

} // namespace oid
