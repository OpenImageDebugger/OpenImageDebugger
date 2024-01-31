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

#include "raw_data_decode.h"

#include <cassert>

std::vector<std::uint8_t>
make_float_buffer_from_double(const std::vector<std::uint8_t>& buff_double)
{
    const std::size_t element_count = buff_double.size() / sizeof(double);
    std::vector<std::uint8_t> buff_float(element_count * sizeof(float));

    // Cast from double to float
    const auto src = reinterpret_cast<const double*>(buff_double.data());
    auto* dst = reinterpret_cast<float*>(buff_float.data());
    for (std::size_t i = 0; i < element_count; ++i) {
        dst[i] = static_cast<float>(src[i]);
    }

    return buff_float;
}


size_t typesize(const BufferType type)
{
    switch (type) {
    case BufferType::Int32:
        return sizeof(int32_t);
    case BufferType::Short:
        [[fallthrough]];
    case BufferType::UnsignedShort:
        return sizeof(int16_t);
    case BufferType::Float32:
        return sizeof(float);
    case BufferType::Float64:
        return sizeof(double);
    case BufferType::UnsignedByte:
        return sizeof(std::uint8_t);
    default:
        assert("unknow BufferType received");
        return sizeof(std::uint8_t);
    }
}
