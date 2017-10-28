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

#ifndef BUFFER_VALUES_H_
#define BUFFER_VALUES_H_

#include <iostream>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "component.h"
#include "visualization/shader.h"

class BufferValues : public Component
{
  public:
    static constexpr float font_size = 96.0f;

    virtual ~BufferValues();

    virtual bool initialize();

    virtual void update()
    {
    }

    virtual int render_index() const;

    virtual void draw(const mat4& projection, const mat4& view_inv);

  private:
    GLuint text_tex;
    GLuint text_vbo;
    FT_Face font;
    FT_Library ft;
    ShaderProgram text_prog;
    float text_pixel_scale = 1.0;
    float text_texture_width, text_texture_height;
    int text_texture_offsets[256][2];
    int text_texture_advances[256][2];
    int text_texture_sizes[256][2];
    int text_texture_tls[256][2];
    static float constexpr padding = 0.125f; // Must be smaller than 0.5

    void generate_glyphs_texture();

    void draw_text(const mat4& projection,
                   const mat4& view_inv,
                   const mat4& buffer_pose,
                   const char* text,
                   float x,
                   float y,
                   float y_offset,
                   float channels);
};

#endif // BUFFER_VALUES_H_
