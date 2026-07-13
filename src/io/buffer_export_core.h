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

#ifndef BUFFER_EXPORT_CORE_H_
#define BUFFER_EXPORT_CORE_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "ipc/raw_data_decode.h"

namespace oid {
class Buffer;
} // namespace oid

namespace oid::BufferExporter {

enum class OutputType { BITMAP, OCTAVE_MATRIX, NUMPY_ARRAY };

struct RgbaImage {
    int width{0};
    int height{0};
    std::vector<std::uint8_t>
        pixels; // width*height*4, RGBA8 rows top-to-bottom
};

// Contrast-normalized RGBA8 buffer. Empty pixels on unknown type.
[[nodiscard]] RgbaImage normalize_to_rgba8(const Buffer& buffer);

// Octave raw-matrix writer. Returns false on stream failure.
bool export_octave(const Buffer& buffer, const std::string& path);

// Buffer geometry/typing, grouped so normalize_to_rgba8_raw() stays under
// Sonar's parameter-count limit. `data` and `bc_comp` stay separate
// parameters: they're the payload being normalized, not buffer shape.
struct RawBufferDesc {
    BufferType type;
    int width;
    int height;
    int channels;
    int step;
    const char* pixel_layout;
};

// Pure entry points taking plain parameters (no GL-coupled Buffer needed),
// so both frontends -- and this file's own tests -- can drive them headlessly.
[[nodiscard]] RgbaImage
normalize_to_rgba8_raw(const std::uint8_t* data,
                       const RawBufferDesc& desc,
                       const std::array<float, 8>& bc_comp);

bool export_octave_raw(const std::uint8_t* data,
                       BufferType type,
                       int width,
                       int height,
                       int channels,
                       int step,
                       const std::string& path);

// NumPy .npy writer. Returns false on stream failure.
bool export_npy(const Buffer& buffer, const std::string& path);

bool export_npy_raw(const std::uint8_t* data,
                    BufferType type,
                    int width,
                    int height,
                    int channels,
                    int step,
                    const std::string& path);

} // namespace oid::BufferExporter

#endif // BUFFER_EXPORT_CORE_H_
