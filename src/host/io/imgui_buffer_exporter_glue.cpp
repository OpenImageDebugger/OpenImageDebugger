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

// The Buffer-taking export_buffer_imgui() lives in its own TU, separate from
// imgui_buffer_exporter.cpp's export_rgba_png(), because it calls
// normalize_to_rgba8(const Buffer&)/export_octave(const Buffer&, ...), whose
// definitions live in io/buffer_export_adapters.cpp (which pulls in
// visualization/components/buffer.h's Component/RenderCanvas/GL types --
// see that file's own header comment). imgui_buffer_exporter_test
// deliberately doesn't compile/link buffer_export_adapters.cpp (mirroring
// buffer_export_core_test's rationale in tests/CMakeLists.txt), so keeping
// this thin wrapper out of imgui_buffer_exporter.cpp keeps that TU's link
// requirements limited to what the test actually exercises
// (export_rgba_png).

#include "host/io/imgui_buffer_exporter.h"

#include <iostream>

namespace oid::host {

bool export_buffer_imgui(const Buffer& buffer,
                         const std::string& path,
                         BufferExporter::OutputType type) {
    try {
        using enum BufferExporter::OutputType;
        switch (type) {
        case BITMAP: {
            const auto image = BufferExporter::normalize_to_rgba8(buffer);
            return export_rgba_png(image, path);
        }
        case OCTAVE_MATRIX:
            return BufferExporter::export_octave(buffer, path);
        case NUMPY_ARRAY:
            return BufferExporter::export_npy(buffer, path);
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[OID] buffer export failed: " << e.what() << '\n';
        return false;
    } catch (...) {
        std::cerr << "[OID] buffer export failed: unknown error\n";
        return false;
    }
}

} // namespace oid::host
