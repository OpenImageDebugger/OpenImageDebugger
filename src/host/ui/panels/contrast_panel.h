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

#ifndef HOST_UI_PANELS_CONTRAST_PANEL_H_
#define HOST_UI_PANELS_CONTRAST_PANEL_H_

namespace oid::host {

class UiState;
class StageManager;
class SvgIconCache;

// Draws the per-channel contrast min/max editor for the selected buffer
// (parity with the legacy Qt frontend's AutoContrastController /
// minMaxEditor grid; see tag legacy-qt). Reads the selected Stage's Buffer via
// `stages.selected_stage(ui.selected())`; draws nothing if there is no
// selection or the Stage/Buffer hasn't initialized yet.
//
// Layout mirrors Qt's gridLayout: a spanning lower/upper-bound icon on the
// left, then two rows (min, max) of up to 4 channel fields (channel icon +
// ImGui::InputFloat, 50px wide, Qt QLineEdit parity), each row ending in a
// reset icon button (Qt's ac_reset_min/ac_reset_max). Edits commit (call
// Buffer::compute_contrast_brightness_parameters()) on
// ImGui::IsItemDeactivatedAfterEdit(), mirroring the Qt UI's
// editingFinished signal. When the buffer is in single-channel display mode
// (Buffer::get_display_channel_mode() == 1), only channel 0's fields are
// enabled; the rest are shown disabled (parity with the Qt app's
// update_channel_labels). The whole editor is disabled when
// `!ui.contrast_enabled()` (Qt's acToggle -> minMaxEditor.setEnabled).
//
// `icons` supplies the Qt-parity lower/upper-bound and per-channel vector
// icons; texture_for() returning 0 (rasterization failed, or no current GL
// context) simply omits that icon rather than falling back to text, since
// the field itself remains usable either way.
//
// Takes `stages` non-const for the same reason draw_toolbar() does:
// StageManager::selected_stage() lazily initializes Stages on first access.
void draw_contrast_panel(const UiState& ui,
                         StageManager& stages,
                         SvgIconCache& icons);

} // namespace oid::host

#endif // HOST_UI_PANELS_CONTRAST_PANEL_H_
