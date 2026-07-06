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

#include "host/io/imgui_buffer_exporter.h"

#include <iostream>

// This is the single translation unit in the app target that provides the
// stb_image_write implementation (function bodies, not just declarations).
// Every other TU that includes <stb_image_write.h> only sees the
// declarations.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace oid::host {

bool export_rgba_png(const oid::BufferExporter::RgbaImage& image,
                     const std::string& path) {
    try {
        const int stride_bytes = image.width * 4;
        if (const int ok = stbi_write_png(path.c_str(),
                                          image.width,
                                          image.height,
                                          4,
                                          image.pixels.data(),
                                          stride_bytes);
            !ok) {
            std::cerr << "[OID] PNG export failed: could not write " << path
                      << '\n';
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[OID] PNG export failed: " << e.what() << '\n';
        return false;
    } catch (...) {
        std::cerr << "[OID] PNG export failed: unknown error\n";
        return false;
    }
}

} // namespace oid::host
