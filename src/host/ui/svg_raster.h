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

#ifndef HOST_UI_SVG_RASTER_H_
#define HOST_UI_SVG_RASTER_H_

#include <cstddef>
#include <vector>

namespace oid::host {

// Rasterizes an in-memory SVG to a tightly-packed RGBA8 buffer of exactly
// out_w x out_h pixels (scale = out_w / svg viewbox width, matching Qt's
// setVectorIcon which renders into a fixed-size pixmap). Returns empty on
// parse/rasterize failure -- never throws.
std::vector<unsigned char> rasterize_svg(const unsigned char* svg_data,
                                         std::size_t svg_size,
                                         int out_w,
                                         int out_h);

} // namespace oid::host

#endif // HOST_UI_SVG_RASTER_H_
