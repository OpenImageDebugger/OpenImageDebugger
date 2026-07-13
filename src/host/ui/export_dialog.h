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

#include <array>
#include <span>
#include <string>
#include <string_view>

#include "io/buffer_export_core.h"

namespace oid::host {

// Transient state seeded by the buffer list's right-click "Export buffer"
// item (open_export_dialog) and consumed by the platform's confirm_export/
// perform_export seam, which shows the native OS save dialog. Carries the
// target buffer_name, the seeded default destination in path_buf, and the
// chosen OutputType format. Owned once by main.cpp and reused across every
// open/close cycle.
struct ExportDialogState {
    bool open{false};
    std::string buffer_name;           // target buffer's variable_name
    std::array<char, 1024> path_buf{}; // fixed-size destination path buffer
    oid::BufferExporter::OutputType format{
        oid::BufferExporter::OutputType::BITMAP};
};

// Composes a default export path with no filesystem checks -- pure string
// composition, safe to unit test without touching disk: `last_export_dir`
// if non-empty, else "<home_env>/Desktop" if `home_env` is non-null and
// non-empty, else ".", then "/<buffer_name>" plus the registry extension
// for `format` (see extension_for).
std::string default_export_path(std::string_view last_export_dir,
                                const char* home_env,
                                const std::string& buffer_name,
                                oid::BufferExporter::OutputType format);

// Opens the dialog for `buffer_name`: resets `st` (format back to the
// BITMAP default) and seeds `path_buf` via default_export_path(
// last_export_dir, getenv("HOME"), buffer_name, st.format).
void open_export_dialog(ExportDialogState& st,
                        const std::string& buffer_name,
                        const std::string& last_export_dir);

// One row per supported export format. `extension` includes the leading dot
// (".png"); `label` is the name shown in the OS save dialog's format filter.
// Stored as const char* so the nfd filter list (which needs null-terminated C
// strings) and the string_view-based helpers below can both use it directly.
// Adding a format is one row in the registry, plus its OutputType enumerator
// and encoder branch in export_buffer_imgui().
struct ExportFormat {
    oid::BufferExporter::OutputType type;
    const char* extension; // ".png"
    const char* label;     // "PNG image"
};

// The export-format registry, in dialog-filter order (first row is the
// default format that classify_export_format() falls back to).
std::span<const ExportFormat> export_formats();

// Returns the extension (with leading dot) for `format`, per the registry;
// returns the default format's extension if `format` is somehow not listed.
std::string_view extension_for(oid::BufferExporter::OutputType format);

// Returns the type of the first export_formats() row whose extension `path`
// ends with (case-insensitively); falls back to the default first row
// (currently BITMAP/PNG) when none matches. Used to derive the export
// format from the
// path chosen in the native save dialog (nfd appends the selected filter's
// extension).
oid::BufferExporter::OutputType classify_export_format(std::string_view path);

// Appends the registry extension for `format` to `path` if it is not
// already there; returns `path` unchanged otherwise. Safety net for when the
// chosen path lacks a recognized extension.
std::string ensure_export_extension(std::string path,
                                    oid::BufferExporter::OutputType format);

// Copies `path` into `st.path_buf` as a null-terminated C string, truncating
// to fit the 1024-byte buffer. Used once a destination path has been
// confirmed; perform_export() reads it back via st.path_buf.data().
void set_export_path(ExportDialogState& st, std::string_view path);

} // namespace oid::host

#endif // HOST_UI_EXPORT_DIALOG_H_
