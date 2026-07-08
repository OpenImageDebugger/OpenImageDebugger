/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "host/app_icon_decode.h"

#include <cstddef>

// This is the single translation unit in this target that provides the
// stb_image implementation (function bodies, not just declarations). The stb
// include dir is a SYSTEM include (-isystem, see src/CMakeLists.txt), which
// suppresses warnings from the vendored header under -Werror.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace oid::host
{

DecodedImage decode_png_rgba(const unsigned char* data, const std::size_t size)
{
    if (data == nullptr || size == 0) {
        return DecodedImage{0, 0, {}};
    }

    int width    = 0;
    int height   = 0;
    int channels = 0;
    constexpr int RGBA = 4;

    unsigned char* pixels =
        stbi_load_from_memory(data,
                               static_cast<int>(size),
                               &width,
                               &height,
                               &channels,
                               RGBA);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return DecodedImage{0, 0, {}};
    }

    const auto byte_count = static_cast<std::size_t>(width) *
                             static_cast<std::size_t>(height) * RGBA;
    DecodedImage image{
        width, height, std::vector<unsigned char>(pixels, pixels + byte_count)};

    stbi_image_free(pixels);

    return image;
}

} // namespace oid::host
