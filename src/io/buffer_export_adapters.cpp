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

#include "buffer_export_core.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

#include "visualization/components/buffer.h"

namespace oid::BufferExporter {

RgbaImage normalize_to_rgba8(const Buffer& buffer) {
    const auto* bc = buffer.auto_buffer_contrast_brightness();
    auto bc_comp = std::array<float, 8>{};
    std::copy_n(bc, 8, bc_comp.begin());

    return normalize_to_rgba8_raw(
        std::bit_cast<const std::uint8_t*>(buffer.buffer().data()),
        {.type = buffer.type(),
         .width = static_cast<int>(buffer.buffer_width_f()),
         .height = static_cast<int>(buffer.buffer_height_f()),
         .channels = buffer.channels(),
         .step = buffer.step(),
         .pixel_layout = buffer.get_pixel_layout()},
        bc_comp);
}

bool export_octave(const Buffer& buffer, const std::string& path) {
    return export_octave_raw(
        std::bit_cast<const std::uint8_t*>(buffer.buffer().data()),
        buffer.type(),
        static_cast<int>(buffer.buffer_width_f()),
        static_cast<int>(buffer.buffer_height_f()),
        buffer.channels(),
        buffer.step(),
        path);
}

bool export_npy(const Buffer& buffer, const std::string& path) {
    return export_npy_raw(
        std::bit_cast<const std::uint8_t*>(buffer.buffer().data()),
        buffer.type(),
        static_cast<int>(buffer.buffer_width_f()),
        static_cast<int>(buffer.buffer_height_f()),
        buffer.channels(),
        buffer.step(),
        path);
}

} // namespace oid::BufferExporter
