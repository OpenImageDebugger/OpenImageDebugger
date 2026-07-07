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

#ifndef HOST_UI_FONTS_H_
#define HOST_UI_FONTS_H_

namespace oid::host {

inline constexpr float UI_FONT_SIZE_BASE = 13.0f; // Qt 10pt at 96dpi

// Fontello glyphs as UTF-8 literals (codepoints match the legacy Qt
// frontend's setFontIcon calls; see tag legacy-qt):
inline constexpr const char* ICON_RECENTER = "\xEE\xA0\x80";       // U+E800
inline constexpr const char* ICON_ROTATE_CCW = "\xEE\xA0\x81";     // U+E801
inline constexpr const char* ICON_ROTATE_CW = "\xEE\xA0\x82";      // U+E802
inline constexpr const char* ICON_AC_EDIT = "\xEE\xA0\x83";        // U+E803
inline constexpr const char* ICON_AC_TOGGLE = "\xEE\xA0\x84";      // U+E804
inline constexpr const char* ICON_LINK_VIEWS = "\xEE\xA0\x85";     // U+E805
inline constexpr const char* ICON_PRECISION_DOWN = "\xEE\xA0\x86"; // U+E806
inline constexpr const char* ICON_PRECISION_UP = "\xEE\xA0\x87";   // U+E807
inline constexpr const char* ICON_AC_RESET = "\xEE\xA0\x88";       // U+E808
inline constexpr const char* ICON_GO_TO = "\xEF\x80\xB1";          // U+F031

// Builds the ImGui font atlas: Roboto (UI font) + merged fontello glyphs,
// rasterized at UI_FONT_SIZE_BASE * content_scale physical px with
// io.FontGlobalScale = 1/content_scale (crisp HiDPI). Call between
// ImGui::CreateContext() and backend init.
void setup_ui_fonts(float content_scale);

} // namespace oid::host

#endif // HOST_UI_FONTS_H_
