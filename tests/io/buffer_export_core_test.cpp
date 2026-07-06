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

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>

#include "io/buffer_export_core.h"
#include "support/scratch_dir.h"

TEST(ExportCore, NormalizeUint8GrayIdentityContrast) {
    // 2x2 single-channel uint8, bc = {1,1,1,1, 0,0,0,0} (gain 1, bias 0),
    // layout "rgba"
    const std::uint8_t px[] = {0, 64, 128, 255};
    const auto img = oid::BufferExporter::normalize_to_rgba8_raw(
        px,
        oid::BufferType::UnsignedByte,
        2,
        2,
        1,
        2,
        {1, 1, 1, 1, 0, 0, 0, 0},
        "rgba");
    ASSERT_EQ(img.width, 2);
    ASSERT_EQ(img.height, 2);
    ASSERT_EQ(img.pixels.size(), 16u);
    // Grayscale repeats ch0 into G,B; A=255.
    const std::uint8_t expected[] = {
        0, 0, 0, 255, 64, 64, 64, 255, 128, 128, 128, 255, 255, 255, 255, 255};
    EXPECT_TRUE(std::equal(img.pixels.begin(), img.pixels.end(), expected));
}

TEST(ExportCore, NormalizeFloatScalesTo255) {
    const std::array<float, 2> px = {0.0f,
                                     1.0f}; // width=2, height=1, 1ch, step=2
    const auto img = oid::BufferExporter::normalize_to_rgba8_raw(
        reinterpret_cast<const std::uint8_t*>(px.data()),
        oid::BufferType::Float32,
        2,
        1,
        1,
        2,
        {1, 1, 1, 1, 0, 0, 0, 0},
        "rgba");
    EXPECT_EQ(img.pixels[0], 0);   // 0.0 * 255
    EXPECT_EQ(img.pixels[4], 255); // 1.0 * 255
}

TEST(ExportCore, OctaveGoldenBytes) {
    const std::uint8_t px[] = {1, 2, 3, 4, 5, 6}; // 3 wide, 2 high, 1ch, step=3
    const auto p = oid::test::scratch_dir() / "oid_p5_golden.oct";
    std::filesystem::remove(p);
    ASSERT_TRUE(oid::BufferExporter::export_octave_raw(
        px, oid::BufferType::UnsignedByte, 3, 2, 1, 3, p.string()));
    std::ifstream is{p, std::ios::binary};
    std::vector<char> got{std::istreambuf_iterator<char>(is), {}};
    // "uint8\n" + int32 LE height=2, width=3, channels=1 + 6 payload bytes
    const char expected[] = {'u', 'i', 'n', 't', '8', '\n', 2, 0, 0, 0, 3, 0,
                             0,   0,   1,   0,   0,   0,    1, 2, 3, 4, 5, 6};
    ASSERT_EQ(got.size(), sizeof(expected));
    EXPECT_TRUE(std::equal(got.begin(), got.end(), expected));
}
