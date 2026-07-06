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

#ifndef HOST_UI_PANELS_STATUS_BAR_H_
#define HOST_UI_PANELS_STATUS_BAR_H_

namespace oid::host {

class UiState;
class BufferModel;
class StageManager;
class GlfwCanvas;

// Draws a single-line status bar (parity with the Qt app's QStatusBar):
// the selected buffer's display name, "WxH", its type/channel label, and
// the current zoom read off the selected Stage's Camera, when one is
// available. Falls back to a neutral "no buffer" line when the model is
// empty or the Stage hasn't initialized yet. When a stage/buffer/camera are
// all available, also appends the pixel coordinate, value and (for float
// buffers) display precision under the last-hovered cursor position, parity
// with the Qt app's MainWindow::update_status_bar. When `ui.status_message()`
// (e.g. the most recent export's outcome) is non-empty, it is
// appended to that line, separated by " | ".
//
// Takes `stages` non-const because StageManager::selected_stage() lazily
// initializes the Stage (and its GL resources) on first access.
void draw_status_bar(const UiState& ui,
                     const BufferModel& model,
                     StageManager& stages,
                     const GlfwCanvas& canvas);

} // namespace oid::host

#endif // HOST_UI_PANELS_STATUS_BAR_H_
