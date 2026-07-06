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

#ifndef HOST_UI_PANELS_ICON_BUTTON_H_
#define HOST_UI_PANELS_ICON_BUTTON_H_

#include <imgui.h>

namespace oid::host {

// Qt-parity icon button: fixed 26px square by default (the legacy Qt
// frontend's QToolButtons were fixed width 26; see tag legacy-qt), fontello
// glyph as label. `checked` (when
// non-null) renders the button in the active color, mirroring a checked
// QToolButton. `size` lets in-row callers match a neighboring widget's
// height (e.g. the AC editor's reset buttons beside its input fields).
inline bool icon_button(const char* glyph,
                        const char* tooltip,
                        const bool* checked = nullptr,
                        const ImVec2& size = ImVec2(26.0f, 26.0f)) {
    const bool on = checked != nullptr && *checked;
    if (on) {
        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    const bool clicked = ImGui::Button(glyph, size);
    if (on) {
        ImGui::PopStyleColor();
    }
    ImGui::SetItemTooltip("%s", tooltip);
    return clicked;
}

} // namespace oid::host

#endif // HOST_UI_PANELS_ICON_BUTTON_H_
