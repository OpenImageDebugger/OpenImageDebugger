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

#include "raw_data_decode.h"

#include <algorithm>

#include <bit>
#include <limits>

namespace oid {

std::vector<std::byte>
make_float_buffer_from_double(const std::vector<std::byte>& buff_double) {
    const auto element_count = buff_double.size() / sizeof(double);
    std::vector<std::byte> buff_float(element_count * sizeof(float));

    const auto src = std::bit_cast<const double*>(buff_double.data());
    const auto dst = std::bit_cast<float*>(buff_float.data());
    std::transform(src, src + element_count, dst, [](const double d) {
        return static_cast<float>(d);
    });

    return buff_float;
}

bool geometry_fits_payload(const int width,
                           const int height,
                           const int channels,
                           const int stride,
                           const BufferType type,
                           const std::size_t byte_count) {
    // type_size() reports one byte for anything it does not recognise, which
    // would silently measure the payload with the wrong element width.
    if (!is_known_buffer_type(type)) {
        return false;
    }
    if (width <= 0 || height <= 0 || channels <= 0 || stride < width) {
        return false;
    }
    const auto element_size =
        static_cast<std::uint64_t>(channels) * type_size(type);
    const auto pixels_needed = (static_cast<std::uint64_t>(height) - 1) *
                                   static_cast<std::uint64_t>(stride) +
                               static_cast<std::uint64_t>(width);
    // Divided rather than multiplied: pixels_needed * element_size can
    // overflow 64 bits for hostile geometry, the quotient cannot.
    return static_cast<std::uint64_t>(byte_count) / element_size >=
           pixels_needed;
}

std::optional<std::size_t> padded_payload_size(const int width,
                                               const int height,
                                               const int channels,
                                               const int stride,
                                               const BufferType type) {
    // As above: an unrecognised type would be sized as one byte per element.
    if (!is_known_buffer_type(type)) {
        return std::nullopt;
    }
    if (width <= 0 || height <= 0 || channels <= 0 || stride < width) {
        return std::nullopt;
    }
    // Each factor is checked before it is applied: hostile geometry can
    // overflow this product, and a wrapped result would look plausible.
    constexpr auto LIMIT = (std::numeric_limits<std::size_t>::max)();
    auto size = static_cast<std::size_t>(stride);
    for (const std::size_t factor : {static_cast<std::size_t>(height),
                                     static_cast<std::size_t>(channels),
                                     type_size(type)}) {
        if (size > LIMIT / factor) {
            return std::nullopt;
        }
        size *= factor;
    }
    return size;
}

} // namespace oid
