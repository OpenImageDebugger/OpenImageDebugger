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

#ifndef HOST_IO_IMGUI_BUFFER_EXPORTER_H_
#define HOST_IO_IMGUI_BUFFER_EXPORTER_H_

#include <string>

#include "io/buffer_export_core.h"

namespace oid {
class Buffer;
} // namespace oid

namespace oid::host {

// Encodes an already contrast-normalized RGBA8 image to a PNG file via
// stb_image_write. Never throws: catches everything internally, logs to
// stderr, and returns false on failure (bad path, encode failure, etc).
[[nodiscard]] bool export_rgba_png(const BufferExporter::RgbaImage& image,
                                   const std::string& path);

// Exports a live Buffer to `path`: BITMAP normalizes to RGBA8 and PNG-encodes
// it; OCTAVE_MATRIX writes the shared raw Octave-matrix format. Never throws.
[[nodiscard]] bool export_buffer_imgui(const Buffer& buffer,
                                       const std::string& path,
                                       BufferExporter::OutputType type);

} // namespace oid::host

#endif // HOST_IO_IMGUI_BUFFER_EXPORTER_H_
