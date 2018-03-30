/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 GDB ImageWatch contributors
 * (github.com/csantosbh/gdb-imagewatch/)
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

#include <QPainter>
#include <QPixmap>

#include "gl_text_renderer.h"
#include "visualization/shaders/giw_shaders.h"


GLTextRenderer::GLTextRenderer(GLCanvas* gl_canvas)
    : font("Times New Roman", font_size)
    , text_prog(gl_canvas)
    , gl_canvas_(gl_canvas)
{
}


GLTextRenderer::~GLTextRenderer()
{
    gl_canvas_->glDeleteTextures(1, &text_tex);
    gl_canvas_->glDeleteBuffers(1, &text_vbo);
}


bool GLTextRenderer::initialize()
{
    text_prog.create(shader::text_vert_shader,
                     shader::text_frag_shader,
                     ShaderProgram::FormatR,
                     "rgba",
                     {"mvp",
                      "buff_sampler",
                      "text_sampler",
                      "pix_coord",
                      "brightness_contrast"});

    gl_canvas_->glGenTextures(1, &text_tex);
    gl_canvas_->glActiveTexture(GL_TEXTURE0);
    gl_canvas_->glBindTexture(GL_TEXTURE_2D, text_tex);
    gl_canvas_->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl_canvas_->glGenBuffers(1, &text_vbo);
    generate_glyphs_texture();

    return true;
}


void GLTextRenderer::generate_glyphs_texture()
{
    // Required characters for numbers, scientific notation (e), nan, inf
    const char text[] = "0123456789., -+enaif";
    const unsigned char* p;
    const int border_size = 0;

    QFontMetrics g(font);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, text_tex);

    // Generate text bitmap
    QSize texture_size(g.size(Qt::TextSingleLine, text));

    text_texture_width = text_texture_height = 1.0f;
    while (text_texture_width < texture_size.width())
        text_texture_width *= 2.f;
    while (text_texture_height < texture_size.height())
        text_texture_height *= 2.f;

    const int mipmap_levels = 5;

    {
        int tex_level_width = text_texture_width;
        int tex_level_height = text_texture_height;

        for (int i = 0; i < mipmap_levels; ++i) {
            gl_canvas_->glTexImage2D(GL_TEXTURE_2D,
                                     i,
                                     GL_R8,
                                     tex_level_width,
                                     tex_level_height,
                                     0,
                                     GL_RED,
                                     GL_UNSIGNED_BYTE,
                                     nullptr);

            tex_level_width = std::max(1, tex_level_width / 2);
            tex_level_height = std::max(1, tex_level_height / 2);
        }
    }

    QPixmap pixmap(texture_size);
    pixmap.fill(QColor(0, 0, 0));
    QPainter painter(&pixmap);
    painter.setPen(QColor(255, 255, 255));
    painter.setFont(font);
    painter.drawText(0, g.ascent(), text);
    QImage img = pixmap.toImage().convertToFormat(QImage::Format_Grayscale8);

    std::vector<uint8_t> packed_texture(text_texture_width *
                                        text_texture_height);
    uint8_t* packed_texture_ptr = packed_texture.data();

    int real_ascent  = texture_size.height() - 1;
    int real_descent = 0;

    bool found_real_descent = false;
    bool found_real_ascent  = false;
    // Pack bitmap and compute real ascent and descent lines
    for (int y = 0; y < texture_size.height(); ++y) {
        uint8_t* imgptr = img.scanLine(y);
        int x;

        bool found_filled_pixel = false;
        for (x = 0; x < texture_size.width(); ++x) {
            packed_texture_ptr[x] = imgptr[x];

            found_filled_pixel = found_filled_pixel || imgptr[x] > 0;
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

        for (; x < text_texture_width; ++x) {
            packed_texture_ptr[x] = 0;
        }
        packed_texture_ptr += static_cast<int>(text_texture_width);
    }


    // Compute text box size
    float box_w = 0;
    float box_h = 0;

    const int cropped_bitmap_height = real_ascent - real_descent;

    for (p = reinterpret_cast<const unsigned char*>(text); *p; p++) {
        int advance_x     = g.width(*p);
        int bitmap_height = g.height();

        text_texture_advances[*p][0] = advance_x;
        text_texture_advances[*p][1] = 0;
        text_texture_sizes[*p][0]    = advance_x;
        text_texture_sizes[*p][1]    = cropped_bitmap_height;
        text_texture_tls[*p][0]      = 0;
        text_texture_tls[*p][1]      = cropped_bitmap_height;

        box_w += advance_x + 2 * border_size;
        box_h = std::max(box_h, (float)bitmap_height + 2 * border_size);
    }

    // Clears generated buffer
    {
        gl_canvas_->glTexSubImage2D(GL_TEXTURE_2D,
                                    0,
                                    0,
                                    0,
                                    text_texture_width,
                                    text_texture_height,
                                    GL_RED,
                                    GL_UNSIGNED_BYTE,
                                    packed_texture.data());
    }

    int x = 0;
    for (p = reinterpret_cast<const unsigned char*>(text); *p; p++) {
        int advance_x = g.width(*p);

        text_texture_offsets[*p][0] = x + border_size;
        text_texture_offsets[*p][1] = real_descent + border_size;

        x += advance_x + border_size * 2;
    }

    gl_canvas_->glGenerateMipmap(GL_TEXTURE_2D);
    gl_canvas_->glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_canvas_->glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    gl_canvas_->glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    gl_canvas_->glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    gl_canvas_->glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
}
