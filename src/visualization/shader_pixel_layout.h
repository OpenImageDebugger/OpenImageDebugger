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

// Returns the pixel layout the buffer fragment shader must be compiled with.
//
// A layout names the channel order of the buffer's data, and the shader takes
// its source component from the layout's first character. A single-channel
// buffer is uploaded as a GL_RED texture, whose samples are (r, 0, 0, 1): only
// the red component carries data. A declared layout starting with 'g', 'b' or
// 'a' would therefore make the shader read a component that is constant zero,
// rendering the whole buffer black at every contrast setting. Channel order is
// meaningless for a single channel, so such a buffer always samples red.
//
// Multi-channel buffers keep their declared layout, including the case where
// the user has selected one channel of an RGB buffer for display: there the
// texture really does carry that component and the first character is what
// selects it.
[[nodiscard]] inline std::string
shader_pixel_layout(const std::string& declared_layout,
                    const int texture_channels) {
    return texture_channels == 1 ? std::string{"rgba"} : declared_layout;
}

// Returns the channel index a layout selects for display, given how many
// channels the buffer actually has.
//
// The layout's first character names the channel, so this must agree with
// shader_pixel_layout() or the viewer reports one channel while rendering
// another. A buffer only carries `texture_channels` components, so a layout
// naming one beyond that resolves to red -- which is what a single-channel
// buffer renders, and the only component a GL_RG texture is guaranteed to
// have when blue is asked for.
//
// This also bounds the loop in BufferValues::draw_pixel_values(), which reads
// the pixel as buffer[pos + channel] without clamping to the channel count.
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
