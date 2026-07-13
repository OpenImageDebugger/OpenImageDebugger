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

// Pure (no ImGui/GL) helpers for composing and validating export paths.

#include "host/ui/export_dialog.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <span>
#include <string_view>

namespace oid::host {

namespace {

// Anonymous-namespace export-format registry; see export_formats() below for
// the public accessor. Adding a format (e.g. NumPy .npy) is one row here.
constexpr std::array<ExportFormat, 2> EXPORT_FORMATS{{
    {oid::BufferExporter::OutputType::BITMAP, ".png", "PNG image"},
    {oid::BufferExporter::OutputType::OCTAVE_MATRIX, ".oct", "Octave matrix"},
}};

bool ends_with(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Copies `s` into `buf` as a null-terminated C string, truncating to fit
// if necessary (path_buf is a fixed-size ImGui InputText backing buffer).
void set_path_buf(std::array<char, 1024>& buf, std::string_view s) {
    const std::size_t n = (std::min)(s.size(), buf.size() - 1);
    std::copy_n(s.begin(), n, buf.begin());
    buf[n] = '\0';
}

} // namespace

std::span<const ExportFormat> export_formats() {
    return EXPORT_FORMATS;
}

std::string_view extension_for(oid::BufferExporter::OutputType format) {
    for (const auto& fmt : EXPORT_FORMATS) {
        if (fmt.type == format) {
            return fmt.extension;
        }
    }
    return EXPORT_FORMATS.front().extension; // PNG
}

std::string default_export_path(std::string_view last_export_dir,
                                const char* home_env,
                                const std::string& buffer_name,
                                oid::BufferExporter::OutputType format) {
    std::string dir;
    if (!last_export_dir.empty()) {
        dir = last_export_dir;
    } else if (home_env != nullptr && home_env[0] != '\0') {
        dir = std::string(home_env) + "/Desktop";
    } else {
        dir = ".";
    }
    if (!dir.empty() && dir.back() == '/') {
        dir.pop_back();
    }
    return dir + "/" + buffer_name + std::string(extension_for(format));
}

void open_export_dialog(ExportDialogState& st,
                        const std::string& buffer_name,
                        const std::string& last_export_dir) {
    st.open = true;
    st.buffer_name = buffer_name;
    // default format = the registry's first row
    st.format = export_formats().front().type;
    set_path_buf(
        st.path_buf,
        default_export_path(
            last_export_dir, std::getenv("HOME"), buffer_name, st.format));
}

oid::BufferExporter::OutputType classify_export_format(std::string_view path) {
    for (const auto& fmt : EXPORT_FORMATS) {
        if (ends_with(path, fmt.extension)) {
            return fmt.type;
        }
    }
    return EXPORT_FORMATS.front().type; // BITMAP/PNG
}

std::string ensure_export_extension(std::string path,
                                    oid::BufferExporter::OutputType format) {
    const std::string_view ext = extension_for(format);
    if (!ends_with(path, ext)) {
        path += ext;
    }
    return path;
}

void set_export_path(ExportDialogState& st, std::string_view path) {
    set_path_buf(st.path_buf, path);
}

} // namespace oid::host
