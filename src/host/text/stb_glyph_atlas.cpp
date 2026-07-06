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

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

// This is the single translation unit that provides the stb_truetype
// implementation for oidwindow and stb_glyph_atlas_test; no other TU
// may define STB_TRUETYPE_IMPLEMENTATION.
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "host/text/fonts/roboto_regular_ttf.h"

namespace oid::host {

std::optional<oid::GlyphAtlas> stb_glyph_atlas() {
    stbtt_fontinfo info;
    if (stbtt_InitFont(&info,
                       fonts::kRobotoRegular,
                       stbtt_GetFontOffsetForIndex(fonts::kRobotoRegular, 0)) ==
        0) {
        return std::nullopt;
    }

    constexpr float pixel_height = 128.0f; // ~Qt's 96pt strip scale
    const float scale = stbtt_ScaleForPixelHeight(&info, pixel_height);
    int ascent{};
    int descent{};
    int line_gap{};
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    const auto baseline = static_cast<int>(ascent * scale + 0.5f);
    const int strip_h = static_cast<int>((ascent - descent) * scale + 0.5f) + 1;

    std::array<int, 256> advances{};
    int strip_w = 0;
    for (const auto* p =
             reinterpret_cast<const unsigned char*>(oid::kGlyphText);
         *p;
         ++p) {
        int adv{};
        int lsb{};
        stbtt_GetCodepointHMetrics(&info, *p, &adv, &lsb);
        advances[*p] = static_cast<int>(adv * scale + 0.5f);
        strip_w += advances[*p];
    }

    std::vector<std::uint8_t> strip(static_cast<std::size_t>(strip_w) * strip_h,
                                    0);
    int pen_x = 0;
    for (const auto* p =
             reinterpret_cast<const unsigned char*>(oid::kGlyphText);
         *p;
         ++p) {
        int x0{};
        int y0{};
        int x1{};
        int y1{};
        stbtt_GetCodepointBitmapBox(
            &info, *p, scale, scale, &x0, &y0, &x1, &y1);
        const int gw = x1 - x0;
        if (const int gh = y1 - y0; gw > 0 && gh > 0) {
            const int dst_x = (std::max)(0, pen_x + x0);
            const int dst_y = std::clamp(baseline + y0, 0, strip_h - gh);
            if (dst_x + gw <= strip_w) {
                std::vector<std::uint8_t> glyph(static_cast<std::size_t>(gw) *
                                                gh);
                stbtt_MakeCodepointBitmap(
                    &info, glyph.data(), gw, gh, gw, scale, scale, *p);
                for (int y = 0; y < gh; ++y) {
                    for (int x = 0; x < gw; ++x) {
                        auto& dst = strip[static_cast<std::size_t>(dst_y + y) *
                                              strip_w +
                                          dst_x + x];
                        dst =
                            (std::max)(dst,
                                       glyph[static_cast<std::size_t>(y) * gw +
                                             x]);
                    }
                }
            }
        }
        pen_x += advances[*p];
    }

    return oid::finalize_strip_atlas(strip.data(), strip_w, strip_h, advances);
}

} // namespace oid::host
