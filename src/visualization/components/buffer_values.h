/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2024 OpenImageDebugger contributors
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

#ifndef BUFFER_VALUES_H_
#define BUFFER_VALUES_H_

#include "component.h"

namespace oid
{

class BufferValues final : public Component
{
  public:
    BufferValues(GameObject* game_object, GLCanvas* gl_canvas);

    ~BufferValues() override;

    void update() override
    {
    }

    [[nodiscard]] int render_index() const override;

    void draw(const mat4& projection, const mat4& view_inv) override;
    void shift_precision_left();
    void shift_precision_right();

  private:
    static float constexpr padding = 0.125f; // Must be smaller than 0.5
    static int constexpr max_float_precision = 10;
    static int constexpr min_float_precision = 3;
    static float constexpr default_text_scale = 1.0f;
    int float_precision                       = min_float_precision;
    float text_pixel_scale                    = default_text_scale;

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

} // namespace oid

#endif // BUFFER_VALUES_H_
