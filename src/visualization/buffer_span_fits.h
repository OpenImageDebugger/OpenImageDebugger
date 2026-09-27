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

#ifndef VISUALIZATION_BUFFER_SPAN_FITS_H_
#define VISUALIZATION_BUFFER_SPAN_FITS_H_

#include <cstddef>
#include <cstdint>

#include "ipc/raw_data_decode.h"

// Deliberately free of GL and canvas headers so the rule can be unit tested
// without a GL context.

namespace oid {

// Not type_size(): make_buffer_record() narrows a FLOAT64 payload to float32
// while leaving the record tagged FLOAT64; eight bytes would reject every one.
[[nodiscard]] constexpr std::size_t
display_element_size(const BufferType type) noexcept {
    using enum BufferType;
    switch (type) {
    case SHORT:
        [[fallthrough]];
    case UNSIGNED_SHORT:
        return sizeof(short);
    case INT32:
        [[fallthrough]];
    case FLOAT32:
        [[fallthrough]];
    case FLOAT64:
        return sizeof(float);
    case UNSIGNED_BYTE:
        [[fallthrough]];
    default:
        return sizeof(unsigned char);
    }
}

// Nothing bounds the draw index, and a file load skips the wire check. The
// last element touched is ((height - 1) * step + width) * channels - 1: the
// final row needs only `width`, so a producer that trims its padding fits.
[[nodiscard]] constexpr bool buffer_span_fits(const int width,
                                              const int height,
                                              const int channels,
                                              const int step,
                                              const std::size_t element_size,
                                              const std::size_t byte_count) {
    if (width <= 0 || height <= 0 || channels <= 0 || step < width ||
        element_size == 0) {
        return false;
    }
    // 32-bit wasm: (height - 1) * step wraps, hiding an undersized buffer.
    const auto bytes_per_pixel = static_cast<std::uint64_t>(channels) *
                                 static_cast<std::uint64_t>(element_size);
    const auto affordable_pixels =
        static_cast<std::uint64_t>(byte_count) / bytes_per_pixel;
    const auto addressed_pixels = (static_cast<std::uint64_t>(height) - 1) *
                                      static_cast<std::uint64_t>(step) +
                                  static_cast<std::uint64_t>(width);
    return affordable_pixels >= addressed_pixels;
}

} // namespace oid

#endif // VISUALIZATION_BUFFER_SPAN_FITS_H_
