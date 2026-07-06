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

#include "host/ui_fonts.h"

#include <array>
#include <cstddef>
#include <cstring>

#include <imgui.h>

#include "host/text/fonts/fontello_ttf.h"
#include "host/text/fonts/roboto_regular_ttf.h"

namespace oid::host {

namespace {

// ImGui frees atlas-owned font data with IM_FREE, so the mutable copy the
// API wants must come from ImGui's own allocator; this replaces passing
// the embedded const arrays through const_cast with ownership disabled.
void* imgui_owned_font_copy(const unsigned char* data, const std::size_t size) {
    void* copy = ImGui::MemAlloc(size);
    IM_ASSERT(copy != nullptr);
    std::memcpy(copy, data, size);
    return copy;
}

} // namespace

void setup_ui_fonts(const float content_scale) {
    ImGuiIO& io = ImGui::GetIO();
    const float px = kUiFontSizeBase * content_scale;

    ImFontConfig roboto_cfg{};
    io.Fonts->AddFontFromMemoryTTF(
        imgui_owned_font_copy(fonts::kRobotoRegular,
                              sizeof(fonts::kRobotoRegular)),
        static_cast<int>(sizeof(fonts::kRobotoRegular)),
        px,
        &roboto_cfg);

    // Fontello icon glyphs merged into the same font (the legacy Qt
    // frontend applied these same codepoints as QToolButton text via
    // setFontIcon; see tag legacy-qt).
    static constexpr std::array<ImWchar, 5> kIconRanges = {
        0xE800, 0xE808, 0xF031, 0xF031, 0};
    ImFontConfig icon_cfg{};
    icon_cfg.MergeMode = true;
    io.Fonts->AddFontFromMemoryTTF(
        imgui_owned_font_copy(fonts::kFontello, sizeof(fonts::kFontello)),
        static_cast<int>(sizeof(fonts::kFontello)),
        px,
        &icon_cfg,
        kIconRanges.data());

    // Rasterized at content_scale, displayed at logical size: crisp HiDPI.
    io.FontGlobalScale = 1.0f / content_scale;
}

} // namespace oid::host
