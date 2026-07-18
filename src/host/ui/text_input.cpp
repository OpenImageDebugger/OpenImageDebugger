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

#include "host/ui/text_input.h"

#include <charconv>
#include <cstddef>

namespace oid::host {

namespace {

// ImGuiInputTextFlags_CallbackResize callback: grows/shrinks the bound
// std::string to match ImGui's internal edit buffer. Same recipe as Dear
// ImGui's own misc/cpp/imgui_stdlib.cpp (not linked into this target, to
// avoid pulling in an extra vendor translation unit for three lines of
// glue).
int resize_callback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(data->UserData);
        str->resize(static_cast<std::size_t>(data->BufTextLen));
        data->Buf = str->data();
    }
    return 0;
}

} // namespace

bool input_text_line(const char* label,
                     std::string& value,
                     ImGuiInputTextFlags flags) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    return ImGui::InputText(label,
                            value.data(),
                            value.capacity() + 1,
                            flags,
                            resize_callback,
                            &value);
}

bool input_text_line_with_hint(const char* label,
                               const char* hint,
                               std::string& value,
                               ImGuiInputTextFlags flags) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    return ImGui::InputTextWithHint(label,
                                    hint,
                                    value.data(),
                                    value.capacity() + 1,
                                    flags,
                                    resize_callback,
                                    &value);
}

bool input_int_field(const char* label, int& value) {
    // Reflect the committed value into a fresh per-call text buffer: while
    // the widget is focused, ImGui edits its own persistent (ID-keyed)
    // buffer and ignores this one, only writing back into it (and returning
    // true) when the text actually changes. So this does not clobber an
    // in-progress edit like "-" on the frames before it parses.
    std::string text = std::to_string(value);
    if (!input_text_line(label, text)) {
        return false;
    }
    if (const std::optional<int> parsed = parse_int_field(text)) {
        value = *parsed;
        return true;
    }
    return false;
}

std::optional<int> parse_int_field(std::string_view text) {
    constexpr std::string_view whitespace = " \t\n\r\f\v";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return std::nullopt;
    }
    const auto last = text.find_last_not_of(whitespace);
    text = text.substr(first, last - first + 1);

    int value = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    if (const auto [ptr, ec] = std::from_chars(begin, end, value);
        ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return value;
}

} // namespace oid::host
