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

#include "ipc/raw_data_decode.h"

// Deliberately free of GL and canvas headers so the rule can be unit tested
// without a GL context.

namespace oid {

// Bytes one element occupies *as the renderer reads it*, which is not always
// what type_size() reports. make_buffer_record() narrows a FLOAT64 payload to
// float32 while leaving the record's type tagged FLOAT64, and both the texture
// upload and the pixel-value overlay then read it back as float. Sizing a
// FLOAT64 record at type_size()'s eight bytes would reject every valid one.
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

// True if `byte_count` bytes can hold every pixel this geometry addresses.
//
// Drawing indexes the buffer as (y * step + x) * channels + c, so the last
// element it can touch is at ((height - 1) * step + width) * channels - 1.
// Nothing on the drawing path bounds that against the span it was given, so a
// buffer describing more pixels than it carries reads past the end of its
// allocation. The wire layer refuses such a buffer on the way in, but a buffer
// can also arrive from a file, and this is the layer that every source shares.
//
// The last row needs only `width` pixels rather than the full `step`, so a
// producer that trims trailing row padding is still accepted.
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
    const auto elements_per_pixel = static_cast<std::size_t>(channels);
    // Divided rather than multiplied: the equivalent product overflows for
    // hostile geometry, the quotient cannot.
    const auto affordable_pixels =
        byte_count / (elements_per_pixel * element_size);
    const auto addressed_pixels = (static_cast<std::size_t>(height) - 1) *
                                      static_cast<std::size_t>(step) +
                                  static_cast<std::size_t>(width);
    return affordable_pixels >= addressed_pixels;
}

} // namespace oid

#endif // VISUALIZATION_BUFFER_SPAN_FITS_H_
