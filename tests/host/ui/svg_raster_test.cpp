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

#include "host/ui/svg_raster.h"

#include <gtest/gtest.h>

#include "host/ui/icons/label_red_channel_svg.h"
#include "host/ui/icons/lower_upper_bound_svg.h"
#include "host/ui/icons/x_svg.h"

namespace {

bool has_nonzero_alpha(const std::vector<unsigned char>& rgba) {
    for (std::size_t i = 3; i < rgba.size(); i += 4) {
        if (rgba[i] != 0) {
            return true;
        }
    }
    return false;
}

TEST(SvgRaster, ChannelIconRasterizesAtQtSize) {
    // The legacy Qt frontend drew channel icons at 10x10 logical (see tag
    // legacy-qt); x2 = a 2.0 content scale.
    const auto px =
        oid::host::rasterize_svg(oid::host::icons::kLabelRedChannelSvg,
                                 sizeof(oid::host::icons::kLabelRedChannelSvg),
                                 20,
                                 20);
    ASSERT_EQ(px.size(), 20u * 20u * 4u);
    EXPECT_TRUE(has_nonzero_alpha(px));
}

TEST(SvgRaster, LowerUpperBoundRasterizesNonSquare) {
    // Qt size 8x35 (setVectorIcon call), x2.
    const auto px =
        oid::host::rasterize_svg(oid::host::icons::kLowerUpperBoundSvg,
                                 sizeof(oid::host::icons::kLowerUpperBoundSvg),
                                 16,
                                 70);
    ASSERT_EQ(px.size(), 16u * 70u * 4u);
    EXPECT_TRUE(has_nonzero_alpha(px));
}

TEST(SvgRaster, GoToXIconRasterizes) {
    const auto px = oid::host::rasterize_svg(
        oid::host::icons::kXSvg, sizeof(oid::host::icons::kXSvg), 20, 20);
    ASSERT_EQ(px.size(), 20u * 20u * 4u);
    EXPECT_TRUE(has_nonzero_alpha(px));
}

TEST(SvgRaster, GarbageInputReturnsEmpty) {
    const unsigned char garbage[] = {'n', 'o', 't', ' ', 's', 'v', 'g'};
    EXPECT_TRUE(
        oid::host::rasterize_svg(garbage, sizeof(garbage), 10, 10).empty());
    EXPECT_TRUE(
        oid::host::rasterize_svg(
            oid::host::icons::kXSvg, sizeof(oid::host::icons::kXSvg), 0, 10)
            .empty());
}

} // namespace
