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

#ifndef HOST_UI_TEXT_INPUT_H_
#define HOST_UI_TEXT_INPUT_H_

#include <imgui.h>

#include <optional>
#include <string>
#include <string_view>

namespace oid::host {

// Thin std::string-friendly wrapper around ImGui::InputText(): ImGui's C API
// takes a fixed char* buffer, so this installs an ImGuiInputTextFlags_
// CallbackResize callback that grows/shrinks `value` to fit as the user
// types, the same recipe as Dear ImGui's own (unused-here) misc/cpp/
// imgui_stdlib.cpp. Returns true the frame the text changes.
bool input_text_line(const char* label,
                     std::string& value,
                     ImGuiInputTextFlags flags = 0);

// Same as input_text_line(), but renders `hint` as greyed-out placeholder
// text while `value` is empty (ImGui::InputTextWithHint), for fields like
// the symbol search box (parity with Qt's QLineEdit::placeholderText).
bool input_text_line_with_hint(const char* label,
                               const char* hint,
                               std::string& value,
                               ImGuiInputTextFlags flags = 0);

// Text field bound to an int: renders the current value, and on a valid,
// fully-numeric edit (see parse_int_field) commits it back to `value` and
// returns true. Invalid/partial input (e.g. mid-edit "-") is left uncommitted
// rather than clobbering `value` with a stale parse.
bool input_int_field(const char* label, int& value);

// Parses `text` as a base-10 signed integer, trimming leading/trailing
// whitespace first. Returns std::nullopt for empty input, non-numeric input,
// or trailing garbage after the number (e.g. "12x").
std::optional<int> parse_int_field(std::string_view text);

} // namespace oid::host

#endif // HOST_UI_TEXT_INPUT_H_
