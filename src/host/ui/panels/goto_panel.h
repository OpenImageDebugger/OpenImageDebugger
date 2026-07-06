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

#ifndef HOST_UI_PANELS_GOTO_PANEL_H_
#define HOST_UI_PANELS_GOTO_PANEL_H_

namespace oid::host {

class UiState;
class StageManager;
class SvgIconCache;

// Draws the "go to pixel" widget (parity with the legacy Qt frontend's
// GoToWidget and its UIEventHandler::toggle_go_to_dialog()/go_to_pixel()
// handlers; see tag legacy-qt) when `open` is true; a no-op otherwise.
// Callers own `open` and are expected to flip it on Ctrl+L (see
// main.cpp).
//
// On the open transition (false -> true), the two integer fields are
// prefilled from the selected Stage's camera center, mirroring
// GoToWidget::set_defaults(): `x = round(camera.get_position().x() - 0.5)`,
// `y = round(camera.get_position().y() - 0.5)`. Camera::get_position()
// returns a buffer-space centered coordinate, so subtracting 0.5 and
// rounding recovers the integer pixel index the camera is centered on.
// Prefill only happens on that transition (not every frame) so it doesn't
// clobber in-progress typing.
//
// Submitting (Enter/Return, or an OK/Go button) validates the fields via
// `ui.parse_goto()`; on success it calls
// `sel->go_to_pixel(x + 0.5f, y + 0.5f)` on the selected Stage (the inverse
// of the prefill math) and closes the widget (`open = false`). Escape
// closes without applying. Null-guards the selected Stage throughout --
// never dereferences a null Stage.
//
// `icons` supplies the Qt-parity x/y vector icons drawn ahead of each
// field; texture_for() returning 0 (rasterization failed, or no current GL
// context) falls back to today's plain "X"/"Y" text label so the widget
// still works.
void draw_goto_panel(const UiState& ui,
                     StageManager& stages,
                     bool& open,
                     SvgIconCache& icons);

} // namespace oid::host

#endif // HOST_UI_PANELS_GOTO_PANEL_H_
