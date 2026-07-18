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

#include "host/text/stb_glyph_atlas.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>

#include "visualization/glyph_atlas.h"

TEST(StbGlyphAtlas, BakesAllGlyphsDeterministically) {
    const auto a = oid::host::stb_glyph_atlas();
    ASSERT_TRUE(a.has_value());
    EXPECT_GT(a->texture_width, 0.0f);
    EXPECT_GT(a->texture_height, 0.0f);
    EXPECT_EQ(a->pixels.size(),
              static_cast<std::size_t>(a->texture_width * a->texture_height));
    // Every charset glyph has positive advance and in-bounds offset.
    for (const auto* p =
             reinterpret_cast<const unsigned char*>(oid::GLYPH_TEXT);
         *p;
         ++p) {
        if (*p == ' ') {
            EXPECT_GT(a->advances[*p][0], 0);
            continue;
        }
        EXPECT_GT(a->advances[*p][0], 0) << *p;
        EXPECT_GE(a->offsets[*p][0], 0);
        EXPECT_LT(static_cast<float>(a->offsets[*p][0]), a->texture_width);
    }
    // Non-empty bitmap (some ink).
    EXPECT_TRUE(std::ranges::any_of(
        a->pixels, [](std::uint8_t const v) { return v > 0; }));
    const auto b = oid::host::stb_glyph_atlas();
    EXPECT_EQ(a->pixels, b->pixels); // deterministic
}
