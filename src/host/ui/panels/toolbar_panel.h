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

#ifndef HOST_UI_PANELS_TOOLBAR_PANEL_H_
#define HOST_UI_PANELS_TOOLBAR_PANEL_H_

namespace oid::host {

class UiState;
class StageManager;
class BufferModel;

// Draws a single horizontal row of icon buttons above the canvas pane,
// matching the legacy Qt frontend's toolbar exactly in order, glyphs, and
// 26x26px sizing (its `toolbar` QGridLayout, minus flip/zoom/export, which
// this frontend does not implement; see tag legacy-qt):
// acEdit, acToggle, reposition_buffer (recenter), linkViewsToggle,
// rotate_90_ccw, rotate_90_cw, go_to_pixel, decrease_float_precision,
// increase_float_precision, followed by the pixel-format combo.
//
// Every action operates on the selected Stage (`stages.selected_stage(
// ui.selected())`) normally, but fans out to every buffer's Stage when
// `ui.link_views()` is true, mirroring the Qt app's link-views behavior.
// Action buttons are disabled (not hidden) when `!ui.has_selection()`; the
// float-precision buttons are additionally disabled unless the selected
// buffer's type is FLOAT32/FLOAT64 (parity with Qt). The three checkable
// buttons (acEdit, acToggle, linkViewsToggle) stay interactive regardless,
// since they're plain UI-state toggles.
//
// `goto_open` is the Ctrl+L go-to-pixel dialog's open flag (owned by the
// caller); the go_to_pixel button sets it to true, mirroring Qt's
// toggle_go_to_dialog() button handler.
//
// Takes `stages` non-const because StageManager::selected_stage()/stage_for()
// lazily initialize Stages (and their GL resources) on first access.
void draw_toolbar(UiState& ui,
                  StageManager& stages,
                  const BufferModel& model,
                  bool& goto_open);

} // namespace oid::host

#endif // HOST_UI_PANELS_TOOLBAR_PANEL_H_
