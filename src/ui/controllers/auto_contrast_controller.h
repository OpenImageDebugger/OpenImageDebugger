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

#ifndef AUTO_CONTRAST_CONTROLLER_H_
#define AUTO_CONTRAST_CONTROLLER_H_

#include <mutex>

namespace oid
{

struct BufferData;
struct UIComponents;
struct WindowState;

class AutoContrastController
{
  public:
    struct Dependencies
    {
        std::recursive_mutex& ui_mutex;
        BufferData& buffer_data;
        WindowState& state;
        UIComponents& ui_components;
    };

    explicit AutoContrastController(const Dependencies& deps);

    void reset_min_labels() const;
    void reset_max_labels() const;

    void ac_c1_min_update();
    void ac_c2_min_update();
    void ac_c3_min_update();
    void ac_c4_min_update();

    void ac_c1_max_update();
    void ac_c2_max_update();
    void ac_c3_max_update();
    void ac_c4_max_update();

    void ac_min_reset();
    void ac_max_reset();

    void ac_toggle(bool is_checked);

  private:
    Dependencies deps_;

    void set_ac_min_value(int idx, float value);
    void set_ac_max_value(int idx, float value);
    void set_ac_value(int idx, float value, bool is_min);
    void reset_color_values(bool is_min);
};

} // namespace oid

#endif // AUTO_CONTRAST_CONTROLLER_H_
