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

#include "glyph_atlas.h"

namespace oid {

GlyphAtlas finalize_strip_atlas(const std::uint8_t* strip,
                                const int strip_w,
                                const int strip_h,
                                const std::array<int, 256>& char_advances,
                                const char* charset) {
    GlyphAtlas atlas;
    constexpr auto border_size = 0;

    atlas.texture_height = 1.0f;
    atlas.texture_width = atlas.texture_height;
    while (atlas.texture_width < static_cast<float>(strip_w)) {
        atlas.texture_width *= 2.0f;
    }
    while (atlas.texture_height < static_cast<float>(strip_h)) {
        atlas.texture_height *= 2.f;
    }

    atlas.pixels.assign(static_cast<std::size_t>(atlas.texture_width) *
                            static_cast<std::size_t>(atlas.texture_height),
                        0);
    auto packed_texture_ptr = atlas.pixels.data();

    auto real_ascent = strip_h - 1;
    auto real_descent = 0;

    auto found_real_descent = false;
    auto found_real_ascent = false;
    // Pack bitmap and compute real ascent and descent lines
    for (int y = 0; y < strip_h; ++y) {
        const auto* row_ptr = strip + static_cast<std::size_t>(y) *
                                          static_cast<std::size_t>(strip_w);
        auto x = 0;

        auto found_filled_pixel = false;
        for (; x < strip_w; ++x) {
            packed_texture_ptr[x] = row_ptr[x];

            found_filled_pixel = found_filled_pixel || row_ptr[x] > 0;
        }

        // If the row was completely empty...
        if (!found_filled_pixel) {
            if (!found_real_descent) {
                real_descent = y;
            } else if (!found_real_ascent) {
                real_ascent = y;
                found_real_ascent = true;
            }
        } else {
            found_real_descent = true;
        }

        for (; x < static_cast<int>(atlas.texture_width); ++x) {
            packed_texture_ptr[x] = 0;
        }
        packed_texture_ptr += static_cast<int>(atlas.texture_width);
    }

    const auto cropped_bitmap_height = real_ascent - real_descent;

    for (const auto* p = reinterpret_cast<const unsigned char*>(charset); *p;
         p++) {
        const auto advance_x = char_advances[*p];

        atlas.advances[*p][0] = advance_x;
        atlas.advances[*p][1] = 0;
        atlas.sizes[*p][0] = advance_x;
        atlas.sizes[*p][1] = cropped_bitmap_height;
        atlas.tls[*p][0] = 0;
        atlas.tls[*p][1] = cropped_bitmap_height;
    }

    auto x = 0;
    for (const auto* p = reinterpret_cast<const unsigned char*>(charset); *p;
         p++) {
        const auto advance_x = char_advances[*p];

        atlas.offsets[*p][0] = x + border_size;
        atlas.offsets[*p][1] = real_descent + border_size;

        x += advance_x + border_size * 2;
    }

    return atlas;
}

GlyphAtlas finalize_strip_atlas(const std::uint8_t* strip,
                                const int strip_w,
                                const int strip_h,
                                const std::array<int, 256>& char_advances) {
    return finalize_strip_atlas(
        strip, strip_w, strip_h, char_advances, kGlyphText);
}

} // namespace oid
