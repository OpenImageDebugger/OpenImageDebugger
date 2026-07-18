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
    constexpr std::uint8_t px[] = {0, 64, 128, 255};
    const auto [width, height, pixels] =
        oid::BufferExporter::normalize_to_rgba8_raw(
            px,
            {.type = oid::BufferType::UNSIGNED_BYTE,
             .width = 2,
             .height = 2,
             .channels = 1,
             .step = 2,
             .pixel_layout = "rgba"},
            {1, 1, 1, 1, 0, 0, 0, 0});
    ASSERT_EQ(width, 2);
    ASSERT_EQ(height, 2);
    ASSERT_EQ(pixels.size(), 16u);
    // Grayscale repeats ch0 into G,B; A=255.
    const std::uint8_t expected[] = {
        0, 0, 0, 255, 64, 64, 64, 255, 128, 128, 128, 255, 255, 255, 255, 255};
    EXPECT_TRUE(std::equal(pixels.begin(), pixels.end(), expected));
}

TEST(ExportCore, NormalizeFloatScalesTo255) {
    constexpr std::array px = {0.0f, 1.0f}; // width=2, height=1, 1ch, step=2
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
    constexpr char expected[] = {'u', 'i', 'n', 't', '8', '\n', 2, 0,
                                 0,   0,   3,   0,   0,   0,    1, 0,
                                 0,   0,   1,   2,   3,   4,    5, 6};
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
    std::vector<char> got{std::istreambuf_iterator(is), {}};

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
    const std::array<std::uint16_t, 6> px = {
        10, 20, 30, 40, 50, 60}; // 3x2, 1ch
    const auto oct = oid::test::scratch_dir() / "oid_cmp.oct";
    const auto npy = oid::test::scratch_dir() / "oid_cmp.npy";
    std::filesystem::remove(oct);
    std::filesystem::remove(npy);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(px.data());
    ASSERT_TRUE(oid::BufferExporter::export_octave_raw(
        bytes, oid::BufferType::UNSIGNED_SHORT, 3, 2, 1, 3, oct.string()));
    ASSERT_TRUE(oid::BufferExporter::export_npy_raw(
        bytes, oid::BufferType::UNSIGNED_SHORT, 3, 2, 1, 3, npy.string()));
    const auto read = [](const std::filesystem::path& p) {
        std::ifstream is{p, std::ios::binary};
        std::vector<std::byte> vector;
        for (std::istreambuf_iterator<char> it{is}, end; it != end; ++it) {
            vector.push_back(static_cast<std::byte>(*it));
        }
        return vector;
    };
    const std::vector<std::byte> o = read(oct);
    const std::vector<std::byte> n = read(npy);
    const auto* payload = reinterpret_cast<const std::byte*>(px.data());
    ASSERT_GE(o.size(), 12u);
    ASSERT_GE(n.size(), 12u);
    EXPECT_TRUE(std::equal(o.end() - 12, o.end(), payload));
    EXPECT_TRUE(std::equal(n.end() - 12, n.end(), payload));
}

TEST(ExportCore, NpyDescrForAllDtypes) {
    const auto check = [](oid::BufferType type,
                          const std::vector<std::uint8_t>& px,
                          int width,
                          int height,
                          const std::string& expected_descr) {
        const auto p = oid::test::scratch_dir() / "oid_descr.npy";
        std::filesystem::remove(p);
        ASSERT_TRUE(oid::BufferExporter::export_npy_raw(
            px.data(), type, width, height, 1, width, p.string()));
        std::ifstream is{p, std::ios::binary};
        const std::string data{std::istreambuf_iterator(is), {}};
        EXPECT_NE(data.find(expected_descr), std::string::npos)
            << "missing " << expected_descr;
    };

    // SHORT, 1x1, itemsize=2.
    check(oid::BufferType::SHORT, {0x34, 0x12}, 1, 1, "'descr': '<i2'");
    // INT32, 2x1, itemsize=4, 2 elements => 8 bytes.
    check(oid::BufferType::INT32,
          {0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00},
          2,
          1,
          "'descr': '<i4'");
    // FLOAT64, 1x1, itemsize=8.
    check(oid::BufferType::FLOAT64,
          {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f},
          1,
          1,
          "'descr': '<f4'");
}

TEST(ExportCore, NpyRoundTripsThroughReader) {
    constexpr std::array px = {1.5f, -2.0f, 3.25f, 4.0f}; // 2x2, 1ch
    const auto p = oid::test::scratch_dir() / "oid_rt.npy";
    std::filesystem::remove(p);
    ASSERT_TRUE(oid::BufferExporter::export_npy_raw(
        reinterpret_cast<const std::uint8_t*>(px.data()),
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
                           reinterpret_cast<const std::byte*>(px.data())));
}

TEST(ExportCore, NpyMultiChannelShapeIs3D) {
    const std::uint8_t px[] = {1, 2, 3, 4, 5, 6, 7, 8}; // 2x2, 2ch, step=2
    const auto p = oid::test::scratch_dir() / "oid_3d.npy";
    std::filesystem::remove(p);
    ASSERT_TRUE(oid::BufferExporter::export_npy_raw(
        px, oid::BufferType::UNSIGNED_BYTE, 2, 2, 2, 2, p.string()));
    std::ifstream is{p, std::ios::binary};
    const std::string data{std::istreambuf_iterator(is), {}};
    EXPECT_NE(data.find("'shape': (2, 2, 2)"), std::string::npos);
}

TEST(ExportCore, NpyMultiChannelRoundTripsThroughReader) {
    const std::uint8_t px[] = {1, 2, 3, 4, 5, 6, 7, 8}; // 2x2, 2ch, step=2
    const auto p = oid::test::scratch_dir() / "oid_rt_3d.npy";
    std::filesystem::remove(p);
    ASSERT_TRUE(oid::BufferExporter::export_npy_raw(
        px, oid::BufferType::UNSIGNED_BYTE, 2, 2, 2, 2, p.string()));
    std::ifstream is{p, std::ios::binary};
    std::vector<std::byte> raw;
    for (std::istreambuf_iterator<char> it{is}, end; it != end; ++it) {
        raw.push_back(static_cast<std::byte>(*it));
    }
    const auto arr = oid::decode_npy(raw);
    ASSERT_TRUE(arr.has_value());
    EXPECT_EQ(arr->type, oid::BufferType::UNSIGNED_BYTE);
    EXPECT_EQ(arr->width, 2);
    EXPECT_EQ(arr->height, 2);
    EXPECT_EQ(arr->channels, 2);
    EXPECT_EQ(arr->bytes.size(), sizeof(px));
    EXPECT_TRUE(std::equal(arr->bytes.begin(),
                           arr->bytes.end(),
                           reinterpret_cast<const std::byte*>(px)));
}
