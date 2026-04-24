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

#ifndef BUFFER_PARAMS_H_
#define BUFFER_PARAMS_H_

#include <array>
#include <span>
#include <string>
#include <vector>

#include "ipc/raw_data_decode.h"

namespace oid
{

namespace BufferConstants
{
constexpr int MAX_TEXTURE_SIZE        = 2048;
constexpr float ZOOM_BORDER_THRESHOLD = 40.0f;
constexpr int MIN_BUFFER_DIMENSION    = 1;
constexpr int MAX_BUFFER_DIMENSION =
    131072; // 2^17 = 128K (closest power of 2 to 100k)
constexpr int MIN_CHANNELS = 1;
constexpr int MAX_CHANNELS = 4;
constexpr std::size_t MAX_BUFFER_SIZE =
    16ULL * 1024ULL * 1024ULL * 1024ULL; // 16GB
} // namespace BufferConstants

struct BufferParams
{
    std::span<const std::byte> buffer;
    int buffer_width_i;
    int buffer_height_i;
    int channels;
    BufferType type;
    int step;
    std::string pixel_layout;
    bool transpose_buffer;
};

} // namespace oid

#endif // BUFFER_PARAMS_H_
