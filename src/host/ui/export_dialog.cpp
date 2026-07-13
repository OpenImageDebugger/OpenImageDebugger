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

// Pure (no ImGui/GL) helpers only -- see export_dialog_draw.cpp for
// draw_export_dialog(), kept in its own TU so this one -- and the unit test
// that links it -- never needs to pull in <imgui.h>.

#include "host/ui/export_dialog.h"

#include <algorithm>
#include <cstdlib>
#include <string_view>

namespace oid::host {

namespace {

constexpr std::string_view PNG_EXT = ".png";
constexpr std::string_view OCT_EXT = ".oct";

std::string_view extension_for(oid::BufferExporter::OutputType format) {
    return format == oid::BufferExporter::OutputType::BITMAP ? PNG_EXT
                                                             : OCT_EXT;
}

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

void apply_format_extension(ExportDialogState& st) {
    if (st.user_edited_path) {
        return;
    }

    std::string path(st.path_buf.data());
    const std::string_view target = extension_for(st.format);

    if (ends_with(path, PNG_EXT) || ends_with(path, OCT_EXT)) {
        const std::string_view current =
            ends_with(path, PNG_EXT) ? PNG_EXT : OCT_EXT;
        if (current != target) {
            path.resize(path.size() - current.size());
            path += target;
        }
    } else {
        path += target;
    }

    set_path_buf(st.path_buf, path);
}

void open_export_dialog(ExportDialogState& st,
                        const std::string& buffer_name,
                        const std::string& last_export_dir) {
    st.open = true;
    st.buffer_name = buffer_name;
    st.user_edited_path = false;
    st.format = oid::BufferExporter::OutputType::BITMAP;
    set_path_buf(
        st.path_buf,
        default_export_path(
            last_export_dir, std::getenv("HOME"), buffer_name, st.format));
}

oid::BufferExporter::OutputType classify_export_format(std::string_view path) {
    return ends_with(path, OCT_EXT)
               ? oid::BufferExporter::OutputType::OCTAVE_MATRIX
               : oid::BufferExporter::OutputType::BITMAP;
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
