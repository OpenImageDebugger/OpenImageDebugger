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

#ifndef BUFFER_VALUES_H_
#define BUFFER_VALUES_H_

#include <array>

#include "component.h"
#include "ipc/raw_data_decode.h"
#include "visualization/components/buffer.h"

namespace oid
{

struct PixelFormatParams
{
    BufferType type;
    const std::byte* buffer;
    int pos;
    int channel;
    int label_length;
    int float_precision;
    char* pix_label;
};

struct DrawPixelValuesParams
{
    int x;
    int y;
    const Buffer& buffer;
    int pos_center_x;
    int pos_center_y;
    const std::array<float, 4>& recenter_factors;
    const mat4& projection;
    const mat4& view_inv;
    const mat4& buffer_pose;
};

struct DrawTextParams
{
    const mat4& projection;
    const mat4& view_inv;
    const mat4& buffer_pose;
    const char* text;
    float x;
    float y;
    float y_offset;
    float channels;
};

class BufferValues final : public Component
{
  public:
    BufferValues(std::shared_ptr<GameObject> game_object,
                 std::shared_ptr<GLCanvas> gl_canvas);

    ~BufferValues() override;

    void update() override
    {
        // Do nothing
    }

    [[nodiscard]] int render_index() const override;

    void draw(const mat4& projection, const mat4& view_inv) override;
    void decrease_float_precision();
    void increase_float_precision();
    int get_float_precision() const;

  private:
    static float constexpr padding_{0.125f}; // Must be smaller than 0.5

    static int constexpr max_float_precision_{10};

    static int constexpr min_float_precision_{3};

    static float constexpr default_text_scale_{1.0f};

    int float_precision_{min_float_precision_};

    float text_pixel_scale_{default_text_scale_};

    void draw_text(const DrawTextParams& params);

    void draw_pixel_values(const DrawPixelValuesParams& params);
};

} // namespace oid

#endif // BUFFER_VALUES_H_
