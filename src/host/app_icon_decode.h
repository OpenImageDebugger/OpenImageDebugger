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

#ifndef APP_ICON_DECODE_H_
#define APP_ICON_DECODE_H_

#include <cstddef>
#include <vector>

namespace oid::host {

// A decoded RGBA8 image. On decode failure width/height are 0 and rgba is
// empty; on success rgba.size() == width * height * 4 (tightly packed, no
// row padding).
struct DecodedImage {
    int width;
    int height;
    std::vector<unsigned char> rgba;
};

// Decodes a PNG (or any stb_image-supported format) held in `data`/`size`
// into a 4-channel RGBA8 buffer. Returns an empty DecodedImage on failure.
DecodedImage decode_png_rgba(const unsigned char* data, std::size_t size);

} // namespace oid::host

#endif // APP_ICON_DECODE_H_
