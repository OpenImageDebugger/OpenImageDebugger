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

#ifndef HOST_UI_PANELS_BUFFER_LIST_PANEL_H_
#define HOST_UI_PANELS_BUFFER_LIST_PANEL_H_

#include <string>

namespace oid {
class Stage;
} // namespace oid

namespace oid::host {

class UiState;
class IpcBufferModel;
class IpcClient;
class StageManager;
class ThumbnailCache;
struct ExportDialogState;

// Draws the left-pane buffer list (parity with the Qt app's buffer list
// widget): one row per buffer, a small icon thumbnail (see ThumbnailCache)
// followed by "display_name / WxH / type_label". Clicking a
// row selects it via UiState::select(); pressing Delete while a row is
// selected (and the list has focus/hover) removes that buffer.
// Right-clicking a row opens a context menu with an "Export buffer" item
// that opens `export_dialog` for that row's buffer via
// open_export_dialog() (see export_dialog.h); the caller is responsible for
// confirming the export via the native save dialog.
//
// Takes `model` non-const and `ipc` so Delete can both notify the debugger
// bridge (ipc.notify_removed()) and drop the buffer locally
// (model.remove()); StageManager is not touched by Delete here -- its
// sync() drops the removed buffer's Stage automatically the next time a
// Stage is requested (see stage_manager.h) -- but is otherwise read here to
// look up each row's Stage* for `thumbs.texture_for()`. `thumbs` caches the
// rendered icon textures across frames; see thumbnail_cache.h for its
// per-frame render budget. `last_export_dir` seeds the export dialog's
// default path (see export_dialog.h's default_export_path()).
void draw_buffer_list(UiState& ui,
                      IpcBufferModel& model,
                      IpcClient& ipc,
                      StageManager& stages,
                      ThumbnailCache& thumbs,
                      ExportDialogState& export_dialog,
                      const std::string& last_export_dir);

} // namespace oid::host

#endif // HOST_UI_PANELS_BUFFER_LIST_PANEL_H_
