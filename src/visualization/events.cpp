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

// The sole KeyboardState implementation. Camera (part of the shared
// visualization layer) polls is_modifier_key_pressed() / is_key_pressed()
// every frame for continuous pan/zoom; this polls ImGui's IO state directly
// (ImGui, in turn, is fed by GLFW's key callbacks), so no explicit event
// routing into this file is needed.

#include "events.h"

#include <imgui.h>

namespace oid {

bool KeyboardState::is_modifier_key_pressed(const ModifierKey key) {
    const ImGuiIO& io = ImGui::GetIO();
    switch (key) {
    case ModifierKey::Alt:
        return io.KeyAlt;
    case ModifierKey::Control:
        return io.KeyCtrl;
    case ModifierKey::Shift:
        return io.KeyShift;
    default:
        return false;
    }
}

bool KeyboardState::is_key_pressed(const Key key) {
    // Suppress camera key input while a text field (e.g. the symbol search box)
    // holds the keyboard: the bare arrow keys now pan the canvas, so without
    // this guard navigating the autocomplete with Up/Down would also pan. When
    // no widget captures the keyboard, the canvas owns these keys.
    if (ImGui::GetIO().WantCaptureKeyboard) {
        return false;
    }
    switch (key) {
    case Key::Left:
        return ImGui::IsKeyDown(ImGuiKey_LeftArrow);
    case Key::Right:
        return ImGui::IsKeyDown(ImGuiKey_RightArrow);
    case Key::Up:
        return ImGui::IsKeyDown(ImGuiKey_UpArrow);
    case Key::Down:
        return ImGui::IsKeyDown(ImGuiKey_DownArrow);
    case Key::Plus:
        // US layout: '+' is Shift+'='; also accept the keypad '+'.
        return ImGui::IsKeyDown(ImGuiKey_Equal) ||
               ImGui::IsKeyDown(ImGuiKey_KeypadAdd);
    case Key::Minus:
        return ImGui::IsKeyDown(ImGuiKey_Minus) ||
               ImGui::IsKeyDown(ImGuiKey_KeypadSubtract);
    default:
        return false;
    }
}

} // namespace oid
