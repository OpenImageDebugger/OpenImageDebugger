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

#ifndef VISUALIZATION_GLYPH_ATLAS_H_
#define VISUALIZATION_GLYPH_ATLAS_H_

#include <array>
#include <cstdint>
#include <vector>

namespace oid {

// std::array<std::array<int, 2>, 256> is int[256][2]
using Array_256_2 = std::array<std::array<int, 2>, 256>;

// Required characters for numbers, scientific notation (e), nan, inf.
inline constexpr auto GLYPH_TEXT = "0123456789., -+enaif";

// Pure CPU-side glyph atlas data -- no GL, no Qt. Produced by
// finalize_strip_atlas() below from a rendered single-line glyph strip, and
// consumed by GLTextRenderer::upload_atlas() to populate a GL texture.
struct GlyphAtlas {
    float texture_width{0};  // pow2 dims
    float texture_height{0}; // pow2 dims
    std::vector<std::uint8_t>
        pixels; // R8, texture_width*texture_height, packed rows

    Array_256_2 offsets{};
    Array_256_2 advances{};
    Array_256_2 sizes{};
    Array_256_2 tls{};
};

// Takes the rendered single-line strip (grayscale8, strip_w x strip_h,
// glyphs laid left-to-right at the given advances) and produces the
// pow2-packed atlas + tables. Table entries are filled only for the
// characters in `charset`.
GlyphAtlas finalize_strip_atlas(const std::uint8_t* strip,
                                int strip_w,
                                int strip_h,
                                const std::array<int, 256>& char_advances,
                                const char* charset);

// Convenience overload: charset = GLYPH_TEXT.
GlyphAtlas finalize_strip_atlas(const std::uint8_t* strip,
                                int strip_w,
                                int strip_h,
                                const std::array<int, 256>& char_advances);

} // namespace oid

#endif // VISUALIZATION_GLYPH_ATLAS_H_
