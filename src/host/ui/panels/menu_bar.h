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

#ifndef HOST_UI_PANELS_MENU_BAR_H_
#define HOST_UI_PANELS_MENU_BAR_H_

namespace oid::host {

// Draws the top-of-viewport main menu bar (ImGui::BeginMainMenuBar), the
// ImGui frontend's parity replacement for the Qt app's QMenuBar. Minimal by
// design: File > Open, File > Quit, and Help > About only, matching this
// phase's scope.
//
// Sets `request_quit` to true when File > Quit is chosen; the caller is
// responsible for actually tearing down the window loop (BeginMainMenuBar
// itself has no notion of "quit"). Sets `request_open` to true when
// File > Open is chosen; the caller is responsible for showing the native
// file dialog and queuing the chosen paths.
void draw_menu_bar(bool& request_quit, bool& request_open);

} // namespace oid::host

#endif // HOST_UI_PANELS_MENU_BAR_H_
