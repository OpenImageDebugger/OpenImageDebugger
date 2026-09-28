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

#ifndef HOST_UI_EXPORT_DIALOG_H_
#define HOST_UI_EXPORT_DIALOG_H_

#include <span>
#include <string>
#include <string_view>

#include "io/buffer_export_core.h"

namespace oid::host {

// Seeded by open_export_dialog and consumed by the platform confirm_export/
// perform_export seam. Owned once by main.cpp, reused across open/close.
struct ExportDialogState {
    bool open{false};
    std::string buffer_name; // target buffer's variable_name
    std::string path;
    BufferExporter::OutputType format{BufferExporter::OutputType::BITMAP};
};

// Composes a default export path with no filesystem checks -- pure string
// composition, safe to unit test without touching disk: `last_export_dir`
// if non-empty, else "<home_env>/Desktop" if `home_env` is non-null and
// non-empty, else ".", then "/<buffer_name>" plus the registry extension
// for `format` (see extension_for).
std::string default_export_path(std::string_view last_export_dir,
                                const char* home_env,
                                const std::string& buffer_name,
                                BufferExporter::OutputType format);

// Opens the dialog for `buffer_name`: resets `st` (format back to the
// BITMAP default) and seeds `path` via default_export_path(
// last_export_dir, getenv("HOME"), buffer_name, st.format).
void open_export_dialog(ExportDialogState& st,
                        const std::string& buffer_name,
                        const std::string& last_export_dir);

// Why a host's "export selected buffer" cannot be served with `buffer_count`
// buffers in the model, or empty when it can. Answered by the viewer: the
// host gets no reply and keeps no copy of which buffer is selected.
std::string_view export_selected_refusal(std::size_t buffer_count);

// const char* rather than std::string: the nfd filter list needs
// null-terminated C strings. A new format is one registry row plus an
// OutputType enumerator and a case in export_buffer_imgui().
struct ExportFormat {
    BufferExporter::OutputType type;
    const char* extension; // ".png"
    const char* label;     // "PNG image", shown in the save dialog's filter
};

// The export-format registry, in dialog-filter order (first row is the
// default format that classify_export_format() falls back to).
std::span<const ExportFormat> export_formats();

// Returns the extension (with leading dot) for `format`, per the registry;
// returns the default format's extension if `format` is somehow not listed.
std::string_view extension_for(BufferExporter::OutputType format);

// Derives the format from the path the native save dialog returned (nfd
// appends the selected filter's extension); falls back to the first row.
BufferExporter::OutputType classify_export_format(std::string_view path);

// Safety net for a confirmed path that carries no recognized extension.
std::string ensure_export_extension(std::string path,
                                    BufferExporter::OutputType format);

} // namespace oid::host

#endif // HOST_UI_EXPORT_DIALOG_H_
