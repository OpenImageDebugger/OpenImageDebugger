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

#ifndef HOST_TEXT_STB_GLYPH_ATLAS_H_
#define HOST_TEXT_STB_GLYPH_ATLAS_H_

#include <optional>

#include "visualization/glyph_atlas.h"

namespace oid::host {

// Bakes oid::GLYPH_TEXT at 128px height from the embedded Roboto-Regular
// font (src/resources/fonts/Roboto-Regular.ttf) via stb_truetype into the
// same strip->finalize_strip_atlas() pipeline the legacy Qt frontend's
// QPainter baker used (see tag legacy-qt), so the ImGui frontend gets a
// Qt-free equivalent atlas for pixel-value text overlays. Returns
// std::nullopt if the embedded font fails to parse (never throws).
std::optional<GlyphAtlas> stb_glyph_atlas();

} // namespace oid::host

#endif // HOST_TEXT_STB_GLYPH_ATLAS_H_
