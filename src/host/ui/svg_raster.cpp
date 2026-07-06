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

#include "host/ui/svg_raster.h"

#include <string>
#include <vector>

// This is the single translation unit in the app target that provides the
// nanosvg implementations (function bodies, not just declarations); same
// one-TU rule as the stb libraries, see src/CMakeLists.txt. nanosvg's
// headers are C and would trip -Wold-style-cast/-Wcast-qual etc under our
// -Werror, but the nanosvg include dir is added as a SYSTEM include
// (-isystem) in src/CMakeLists.txt — same as the stb include dir — which
// suppresses warnings from those headers. MSVC is the exception: its
// SYSTEM-include analogue (/external:W0) only covers front-end warnings,
// so code-generation warnings like C4702 (unreachable code at /O2) still
// fire inside these headers and need an explicit suppression.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace oid::host {

std::vector<unsigned char> rasterize_svg(const unsigned char* svg_data,
                                         const std::size_t svg_size,
                                         const int out_w,
                                         const int out_h) {
    if (svg_data == nullptr || svg_size == 0 || out_w <= 0 || out_h <= 0) {
        return {};
    }

    // nsvgParse mutates its input and expects NUL termination.
    std::string buf(reinterpret_cast<const char*>(svg_data), svg_size);
    NSVGimage* image = nsvgParse(buf.data(), "px", 96.0f);
    if (image == nullptr || image->width <= 0.0f || image->height <= 0.0f ||
        image->shapes == nullptr) {
        if (image != nullptr) {
            nsvgDelete(image);
        }
        return {};
    }

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    if (rast == nullptr) {
        nsvgDelete(image);
        return {};
    }

    std::vector<unsigned char> rgba(static_cast<std::size_t>(out_w) *
                                    static_cast<std::size_t>(out_h) * 4);
    // Qt's setVectorIcon renders the whole SVG into a w x h pixmap; the
    // target sizes are chosen to match each icon's aspect, so a single
    // x-derived scale fills the buffer the same way.
    const float scale = static_cast<float>(out_w) / image->width;
    nsvgRasterize(
        rast, image, 0.0f, 0.0f, scale, rgba.data(), out_w, out_h, out_w * 4);

    nsvgDeleteRasterizer(rast);
    nsvgDelete(image);
    return rgba;
}

} // namespace oid::host
