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
#include <string>
#include <string_view>

#include "io/buffer_export_core.h"

namespace oid::host {

// State for the "Export buffer" modal opened from the buffer list's
// right-click context menu: which buffer to export, the
// destination path (backing an ImGui InputText, hence the fixed-size
// buffer), and which BufferExporter::OutputType to write. Owned once by
// main.cpp and reused across every open/close cycle.
struct ExportDialogState {
    bool open{false};
    std::string buffer_name;           // target buffer's variable_name
    std::array<char, 1024> path_buf{}; // ImGui InputText storage
    oid::BufferExporter::OutputType format{
        oid::BufferExporter::OutputType::Bitmap};
    // True once the user has typed into path_buf directly; gates
    // apply_format_extension() so switching the format radio button never
    // clobbers a path the user has customized themselves.
    bool user_edited_path{false};
};

// Composes a default export path with no filesystem checks -- pure string
// composition, safe to unit test without touching disk: `last_export_dir`
// if non-empty, else "<home_env>/Desktop" if `home_env` is non-null and
// non-empty, else ".", then "/<buffer_name>" plus ".png" for
// OutputType::Bitmap or ".oct" for OutputType::OctaveMatrix.
std::string default_export_path(std::string_view last_export_dir,
                                const char* home_env,
                                const std::string& buffer_name,
                                oid::BufferExporter::OutputType format);

// Keeps `st.path_buf`'s extension in sync with `st.format`: replaces a
// trailing ".png"/".oct" with the extension matching the current format,
// or appends the extension if the path has neither. A no-op once the user
// has edited the path directly (`st.user_edited_path == true`).
void apply_format_extension(ExportDialogState& st);

// Opens the dialog for `buffer_name`: resets `st` (format back to the
// Bitmap default, user_edited_path cleared) and seeds `path_buf` via
// default_export_path(last_export_dir, getenv("HOME"), buffer_name,
// st.format).
void open_export_dialog(ExportDialogState& st,
                        const std::string& buffer_name,
                        const std::string& last_export_dir);

// Draws the "Export buffer" ImGui modal popup (implemented in
// export_dialog_draw.cpp): a path field, Bitmap/OctaveMatrix radio
// buttons, and Save/Cancel buttons.
// Returns true the one frame Save is clicked (the destination path is
// st.path_buf.data() at that point); closes the popup on both Save and
// Cancel. A no-op (returns false) while `st.open` is false.
bool draw_export_dialog(ExportDialogState& st);

} // namespace oid::host

#endif // HOST_UI_EXPORT_DIALOG_H_
