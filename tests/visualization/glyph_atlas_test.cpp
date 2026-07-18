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

#include "visualization/glyph_atlas.h"

#include <array>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

TEST(GlyphAtlas, FinalizeComputesPow2AndTables) {
    // Strip 6x4: one fake glyph 'A' of advance 6; rows 0 and 3 empty,
    // rows 1-2 filled. Scan semantics: the first empty row (y=0) sets
    // real_descent=0; a later empty row after filled rows (y=3) sets
    // real_ascent=3 -> cropped height = 3 - 0 = 3.
    std::vector<std::uint8_t> strip(6 * 4, 0);
    for (int x = 0; x < 6; ++x) {
        strip[6 * 1 + x] = 255;
        strip[6 * 2 + x] = 200;
    }
    std::array<int, 256> adv{};
    adv['A'] = 6;
    // GLYPH_TEXT drives table fill; for the test call a charset-parameterized
    // overload finalize_strip_atlas(strip, w, h, adv, "A"):
    const auto atlas = oid::finalize_strip_atlas(strip.data(), 6, 4, adv, "A");
    EXPECT_EQ(atlas.texture_width, 8.0f);  // pow2 of 6
    EXPECT_EQ(atlas.texture_height, 4.0f); // pow2 of 4
    EXPECT_EQ(atlas.pixels.size(), 8u * 4u);
    EXPECT_EQ(atlas.pixels[8 * 1 + 0], 255); // packed row content preserved
    EXPECT_EQ(atlas.advances['A'][0], 6);
    EXPECT_EQ(atlas.offsets['A'][0], 0);
    EXPECT_EQ(atlas.offsets['A'][1], 0); // real_descent baseline
    EXPECT_EQ(atlas.sizes['A'][1], 3);   // cropped height = real_ascent-descent
}

TEST(GlyphAtlas, DeterministicAcrossTwoCalls) {
    const std::vector<std::uint8_t> strip(4 * 2, 7);
    std::array<int, 256> adv{};
    adv['1'] = 4;
    const auto a = oid::finalize_strip_atlas(strip.data(), 4, 2, adv, "1");
    const auto b = oid::finalize_strip_atlas(strip.data(), 4, 2, adv, "1");
    EXPECT_EQ(a.pixels, b.pixels);
    EXPECT_EQ(a.offsets, b.offsets);
}
