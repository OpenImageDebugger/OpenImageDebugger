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

#include "host/io/npy_decode.h"

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "npy_test_blob.h"

using namespace oid;
using oid::test::make_npy;
using oid::test::u8_payload;

TEST(NpyDecodeTest, DecodesCOrder2DUint8) {
    const auto blob = make_npy("|u1", false, {3, 4}, u8_payload(12));
    const auto result = decode_npy(blob);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->type, BufferType::UNSIGNED_BYTE);
    EXPECT_EQ(result->height, 3);
    EXPECT_EQ(result->width, 4);
    EXPECT_EQ(result->channels, 1);
    EXPECT_EQ(result->step, 4);
    EXPECT_FALSE(result->transpose);
    EXPECT_EQ(result->bytes.size(), 12u);
}

TEST(NpyDecodeTest, DecodesCOrder3DUint8Rgb) {
    const auto blob = make_npy("|u1", false, {2, 5, 3}, u8_payload(30));
    const auto result = decode_npy(blob);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->height, 2);
    EXPECT_EQ(result->width, 5);
    EXPECT_EQ(result->channels, 3);
    EXPECT_EQ(result->step, 5);
}

TEST(NpyDecodeTest, DecodesFloat32) {
    const auto blob = make_npy("<f4", false, {2, 2}, u8_payload(16));
    const auto result = decode_npy(blob);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->type, BufferType::FLOAT32);
    EXPECT_EQ(result->bytes.size(), 16u);
}

TEST(NpyDecodeTest, DecodesFloat64KeepsDoubleBytes) {
    const auto blob = make_npy("<f8", false, {2, 2}, u8_payload(32));
    const auto result = decode_npy(blob);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->type, BufferType::FLOAT64);
    EXPECT_EQ(result->bytes.size(), 32u); // raw doubles, not yet float
}

TEST(NpyDecodeTest, FortranOrder2DSwapsAndTransposes) {
    const auto blob = make_npy("|u1", true, {4, 3}, u8_payload(12));
    const auto result = decode_npy(blob);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->width, 4);
    EXPECT_EQ(result->height, 3);
    EXPECT_EQ(result->step, 4);
    EXPECT_TRUE(result->transpose);
}

TEST(NpyDecodeTest, RejectsBigEndian) {
    const auto blob = make_npy(">f4", false, {2, 2}, u8_payload(16));
    const auto result = decode_npy(blob);
    EXPECT_FALSE(result.has_value());
}

TEST(NpyDecodeTest, RejectsFortran3D) {
    const auto blob = make_npy("|u1", true, {2, 5, 3}, u8_payload(30));
    const auto result = decode_npy(blob);
    EXPECT_FALSE(result.has_value());
}

TEST(NpyDecodeTest, RejectsRank1) {
    const auto blob = make_npy("|u1", false, {12}, u8_payload(12));
    const auto result = decode_npy(blob);
    EXPECT_FALSE(result.has_value());
}

TEST(NpyDecodeTest, RejectsPayloadSizeMismatch) {
    const auto blob = make_npy("|u1", false, {3, 4}, u8_payload(11));
    const auto result = decode_npy(blob);
    EXPECT_FALSE(result.has_value());
}

TEST(NpyDecodeTest, RejectsUnsupportedDtype) {
    const auto blob = make_npy("<c8", false, {2, 2}, u8_payload(32));
    const auto result = decode_npy(blob);
    EXPECT_FALSE(result.has_value());
}

TEST(NpyDecodeTest, RejectsTruncatedMagic) {
    std::vector blob = {static_cast<std::byte>(0x93),
                        static_cast<std::byte>('N')};
    const auto result = decode_npy(blob);
    EXPECT_FALSE(result.has_value());
}
