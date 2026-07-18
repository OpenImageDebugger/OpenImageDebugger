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

#ifndef HOST_UI_SHORTCUTS_H_
#define HOST_UI_SHORTCUTS_H_

namespace oid::host {

// True when a modified application shortcut should fire this frame. The
// modifier is Ctrl (or Cmd/Super on macOS -- the caller passes whichever
// applies) combined with a key, which is never text input, so -- unlike the
// plain keyboard-pan keys -- it must fire even while an ImGui text field holds
// keyboard focus, matching the legacy Qt frontend's global QShortcuts (see
// tag legacy-qt). It is deliberately NOT gated on
// ImGui::GetIO().WantCaptureKeyboard, which is true whenever any text field
// (including the symbol search box) is focused.
inline bool should_fire_ctrl_shortcut(const bool key_ctrl,
                                      const bool key_pressed) {
    return key_ctrl && key_pressed;
}

} // namespace oid::host

#endif // HOST_UI_SHORTCUTS_H_
