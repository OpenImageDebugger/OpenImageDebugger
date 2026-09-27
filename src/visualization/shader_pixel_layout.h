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

#ifndef VISUALIZATION_SHADER_PIXEL_LAYOUT_H_
#define VISUALIZATION_SHADER_PIXEL_LAYOUT_H_

#include <string>

// Deliberately free of GL and canvas headers so the rule can be unit tested
// without a GL context.

namespace oid {

// GL_RED samples as (r, 0, 0, 1), so a 'g'/'b'/'a' layout would read zero.
[[nodiscard]] inline std::string
shader_pixel_layout(const std::string& declared_layout,
                    const int texture_channels) {
    return texture_channels == 1 ? std::string{"rgba"} : declared_layout;
}

// Must agree with shader_pixel_layout(); 'b' on a GL_RG texture resolves to
// red. The clamp below is the only bound on draw_pixel_values()'s unchecked
// buffer[pos + channel].
[[nodiscard]] inline int selected_channel_index(const std::string& layout,
                                                const int texture_channels) {
    if (layout.empty()) {
        return 0;
    }
    auto index = 0;
    if (layout[0] == 'g') {
        index = 1;
    } else if (layout[0] == 'b') {
        index = 2;
    }
    return index < texture_channels ? index : 0;
}

} // namespace oid

#endif // VISUALIZATION_SHADER_PIXEL_LAYOUT_H_
