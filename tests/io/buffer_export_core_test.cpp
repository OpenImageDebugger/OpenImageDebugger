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

#include "host/io/npy_decode.h"
#include "io/buffer_export_core.h"
#include "support/scratch_dir.h"

TEST(ExportCore, NormalizeUint8GrayIdentityContrast) {
    // 2x2 single-channel uint8, bc = {1,1,1,1, 0,0,0,0} (gain 1, bias 0),
    // layout "rgba"
    const std::uint8_t px[] = {0, 64, 128, 255};
    const auto img = oid::BufferExporter::normalize_to_rgba8_raw(
        px,
        {.type = oid::BufferType::UNSIGNED_BYTE,
         .width = 2,
         .height = 2,
         .channels = 1,
         .step = 2,
         .pixel_layout = "rgba"},
        {1, 1, 1, 1, 0, 0, 0, 0});
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
        {.type = oid::BufferType::FLOAT32,
         .width = 2,
         .height = 1,
         .channels = 1,
         .step = 2,
         .pixel_layout = "rgba"},
        {1, 1, 1, 1, 0, 0, 0, 0});
    EXPECT_EQ(img.pixels[0], 0);   // 0.0 * 255
    EXPECT_EQ(img.pixels[4], 255); // 1.0 * 255
}

TEST(ExportCore, OctaveGoldenBytes) {
    const std::uint8_t px[] = {1, 2, 3, 4, 5, 6}; // 3 wide, 2 high, 1ch, step=3
    const auto p = oid::test::scratch_dir() / "oid_p5_golden.oct";
    std::filesystem::remove(p);
    ASSERT_TRUE(oid::BufferExporter::export_octave_raw(
        px, oid::BufferType::UNSIGNED_BYTE, 3, 2, 1, 3, p.string()));
    std::ifstream is{p, std::ios::binary};
    std::vector<char> got{std::istreambuf_iterator<char>(is), {}};
    // "uint8\n" + int32 LE height=2, width=3, channels=1 + 6 payload bytes
    const char expected[] = {'u', 'i', 'n', 't', '8', '\n', 2, 0, 0, 0, 3, 0,
                             0,   0,   1,   0,   0,   0,    1, 2, 3, 4, 5, 6};
    ASSERT_EQ(got.size(), sizeof(expected));
    EXPECT_TRUE(std::equal(got.begin(), got.end(), expected));
}

TEST(ExportCore, NpyGoldenBytes) {
    const std::uint8_t px[] = {1, 2, 3, 4, 5, 6}; // 3 wide, 2 high, 1ch, step=3
    const auto p = oid::test::scratch_dir() / "oid_npy_golden.npy";
    std::filesystem::remove(p);
    ASSERT_TRUE(oid::BufferExporter::export_npy_raw(
        px, oid::BufferType::UNSIGNED_BYTE, 3, 2, 1, 3, p.string()));
    std::ifstream is{p, std::ios::binary};
    std::vector<char> got{std::istreambuf_iterator<char>(is), {}};

    std::string expected;
    expected.append("\x93NUMPY", 6);
    expected.push_back('\x01');
    expected.push_back('\x00');
    expected.push_back('\x76'); // header_len = 118, little-endian
    expected.push_back('\x00');
    expected += "{'descr': '|u1', 'fortran_order': False, 'shape': (2, 3), }";
    expected.append(58, ' ');
    expected.push_back('\n');
    const char payload[] = {1, 2, 3, 4, 5, 6};
    expected.append(payload, sizeof(payload));

    ASSERT_EQ(got.size(), expected.size()); // 134
    EXPECT_TRUE(std::equal(got.begin(), got.end(), expected.begin()));
}

TEST(ExportCore, NpyPayloadEqualsOctavePayload) {
    // .npy and .oct share the same tightly-packed raw data region.
    const std::uint16_t px[] = {10, 20, 30, 40, 50, 60}; // 3x2, 1ch, step=3
    const auto oct = oid::test::scratch_dir() / "oid_cmp.oct";
    const auto npy = oid::test::scratch_dir() / "oid_cmp.npy";
    std::filesystem::remove(oct);
    std::filesystem::remove(npy);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(px);
    ASSERT_TRUE(oid::BufferExporter::export_octave_raw(
        bytes, oid::BufferType::UNSIGNED_SHORT, 3, 2, 1, 3, oct.string()));
    ASSERT_TRUE(oid::BufferExporter::export_npy_raw(
        bytes, oid::BufferType::UNSIGNED_SHORT, 3, 2, 1, 3, npy.string()));
    const auto read = [](const std::filesystem::path& p) {
        std::ifstream is{p, std::ios::binary};
        return std::vector<char>{std::istreambuf_iterator<char>(is), {}};
    };
    const std::vector<char> o = read(oct);
    const std::vector<char> n = read(npy);
    const char* payload = reinterpret_cast<const char*>(px);
    EXPECT_TRUE(std::equal(o.end() - 12, o.end(), payload));
    EXPECT_TRUE(std::equal(n.end() - 12, n.end(), payload));
}

TEST(ExportCore, NpyRoundTripsThroughReader) {
    const float px[] = {1.5f, -2.0f, 3.25f, 4.0f}; // 2x2, 1ch, step=2
    const auto p = oid::test::scratch_dir() / "oid_rt.npy";
    std::filesystem::remove(p);
    ASSERT_TRUE(oid::BufferExporter::export_npy_raw(
        reinterpret_cast<const std::uint8_t*>(px),
        oid::BufferType::FLOAT32,
        2,
        2,
        1,
        2,
        p.string()));
    std::ifstream is{p, std::ios::binary};
    std::vector<std::byte> raw;
    for (std::istreambuf_iterator<char> it{is}, end; it != end; ++it) {
        raw.push_back(static_cast<std::byte>(*it));
    }
    const auto arr = oid::decode_npy(raw);
    ASSERT_TRUE(arr.has_value());
    EXPECT_EQ(arr->type, oid::BufferType::FLOAT32);
    EXPECT_EQ(arr->width, 2);
    EXPECT_EQ(arr->height, 2);
    EXPECT_EQ(arr->channels, 1);
    EXPECT_EQ(arr->bytes.size(), sizeof(px));
    EXPECT_TRUE(std::equal(arr->bytes.begin(),
                           arr->bytes.end(),
                           reinterpret_cast<const std::byte*>(px)));
}

TEST(ExportCore, NpyMultiChannelShapeIs3D) {
    const std::uint8_t px[] = {1, 2, 3, 4, 5, 6, 7, 8}; // 2x2, 2ch, step=2
    const auto p = oid::test::scratch_dir() / "oid_3d.npy";
    std::filesystem::remove(p);
    ASSERT_TRUE(oid::BufferExporter::export_npy_raw(
        px, oid::BufferType::UNSIGNED_BYTE, 2, 2, 2, 2, p.string()));
    std::ifstream is{p, std::ios::binary};
    const std::string data{std::istreambuf_iterator<char>(is), {}};
    EXPECT_NE(data.find("'shape': (2, 2, 2)"), std::string::npos);
}
