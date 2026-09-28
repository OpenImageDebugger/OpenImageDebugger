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

#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <numbers>
#include <vector>

#include "ipc/raw_data_decode.h"

using namespace oid;

namespace {
constexpr double TEST_PI = std::numbers::pi;
constexpr double TEST_LARGE_VALUE = 1e10;
constexpr double TEST_NEGATIVE_VALUE = -42.5;
constexpr double TEST_VALUE_1 = 1.0;
constexpr double TEST_VALUE_2_5 = 2.5;
constexpr double TEST_VALUE_4 = 4.0;
constexpr double TEST_VALUE_5 = 5.0;
} // namespace

TEST(RawDataDecodeTest, TypeSizeUnsignedByte) {
    EXPECT_EQ(type_size(BufferType::UNSIGNED_BYTE), sizeof(std::uint8_t));
}

TEST(RawDataDecodeTest, TypeSizeUnsignedShort) {
    EXPECT_EQ(type_size(BufferType::UNSIGNED_SHORT), sizeof(std::int16_t));
}

TEST(RawDataDecodeTest, TypeSizeShort) {
    EXPECT_EQ(type_size(BufferType::SHORT), sizeof(std::int16_t));
}

TEST(RawDataDecodeTest, TypeSizeInt32) {
    EXPECT_EQ(type_size(BufferType::INT32), sizeof(std::int32_t));
}

TEST(RawDataDecodeTest, TypeSizeFloat32) {
    EXPECT_EQ(type_size(BufferType::FLOAT32), sizeof(float));
}

TEST(RawDataDecodeTest, TypeSizeFloat64) {
    EXPECT_EQ(type_size(BufferType::FLOAT64), sizeof(double));
}

TEST(RawDataDecodeTest, MakeFloatBufferFromDouble_Empty) {
    constexpr std::vector<std::byte> empty;
    const auto result = make_float_buffer_from_double(empty);
    EXPECT_TRUE(result.empty());
}

namespace {
void TestSingleDoubleValue(const double value) {
    std::vector<std::byte> double_buffer(sizeof(double));
    std::memcpy(double_buffer.data(), &value, sizeof(double));
    const auto float_buffer = make_float_buffer_from_double(double_buffer);
    EXPECT_EQ(float_buffer.size(), sizeof(float));
    float result = 0.0f;
    std::memcpy(&result, float_buffer.data(), sizeof(float));
    EXPECT_FLOAT_EQ(result, static_cast<float>(value));
}
} // namespace

TEST(RawDataDecodeTest, MakeFloatBufferFromDouble_SingleValue) {
    TestSingleDoubleValue(TEST_PI);
}

TEST(RawDataDecodeTest, MakeFloatBufferFromDouble_MultipleValues) {
    const std::vector values = {
        TEST_VALUE_1, TEST_VALUE_2_5, TEST_PI, TEST_VALUE_4, TEST_VALUE_5};
    std::vector<std::byte> double_buffer(values.size() * sizeof(double));
    std::memcpy(double_buffer.data(), values.data(), double_buffer.size());
    const auto float_buffer = make_float_buffer_from_double(double_buffer);

    EXPECT_EQ(float_buffer.size(), values.size() * sizeof(float));
    for (auto i = 0U; i < values.size(); ++i) {
        float result = 0.0f;
        std::memcpy(
            &result, float_buffer.data() + i * sizeof(float), sizeof(float));
        EXPECT_FLOAT_EQ(result, static_cast<float>(values[i]));
    }
}

TEST(RawDataDecodeTest, MakeFloatBufferFromDouble_LargeValue) {
    TestSingleDoubleValue(TEST_LARGE_VALUE);
}

TEST(RawDataDecodeTest, MakeFloatBufferFromDouble_NegativeValue) {
    TestSingleDoubleValue(TEST_NEGATIVE_VALUE);
}

TEST(RawDataDecodeTest, MakeFloatBufferFromDouble_Zero) {
    TestSingleDoubleValue(0.0);
}

TEST(RawDataDecodeTest, GeometryFitsPayloadAcceptsExactLowerBound) {
    // 2 full strides of 6 plus a 4-wide last row: 16 px x 2 ch x 4 B = 128.
    // A fully padded buffer needs 144, so 128 is the trimmed-tail tolerance.
    EXPECT_TRUE(geometry_fits_payload(4, 3, 2, 6, BufferType::FLOAT32, 128));
}

TEST(RawDataDecodeTest, GeometryFitsPayloadRejectsOneByteShortOfLowerBound) {
    EXPECT_FALSE(geometry_fits_payload(4, 3, 2, 6, BufferType::FLOAT32, 127));
}

TEST(RawDataDecodeTest, GeometryFitsPayloadAcceptsStrideEqualToWidth) {
    // stride == width is the tightest legal packing (no row padding at all).
    EXPECT_TRUE(
        geometry_fits_payload(4, 2, 1, 4, BufferType::UNSIGNED_BYTE, 8));
}

TEST(RawDataDecodeTest, GeometryFitsPayloadUsesWireByteCountForFloat64) {
    // FLOAT64 wire elements are 8 bytes and make_buffer_record() narrows
    // them later, so the check must size the wire, not the narrowed form.
    EXPECT_TRUE(geometry_fits_payload(2, 2, 1, 2, BufferType::FLOAT64, 32));
    EXPECT_FALSE(geometry_fits_payload(2, 2, 1, 2, BufferType::FLOAT64, 31));
}

TEST(RawDataDecodeTest, WithinDisplayLimitsAcceptsBoundaryValues) {
    EXPECT_TRUE(within_display_limits(
        MIN_BUFFER_DIMENSION, MIN_BUFFER_DIMENSION, MIN_CHANNEL_COUNT));
    EXPECT_TRUE(within_display_limits(
        MAX_BUFFER_DIMENSION, MAX_BUFFER_DIMENSION, MAX_CHANNEL_COUNT));
}

TEST(RawDataDecodeTest, WithinDisplayLimitsRejectsOnePastEachBound) {
    EXPECT_FALSE(within_display_limits(
        MIN_BUFFER_DIMENSION - 1, MIN_BUFFER_DIMENSION, MIN_CHANNEL_COUNT));
    EXPECT_FALSE(within_display_limits(
        MIN_BUFFER_DIMENSION, MIN_BUFFER_DIMENSION - 1, MIN_CHANNEL_COUNT));
    EXPECT_FALSE(within_display_limits(
        MIN_BUFFER_DIMENSION, MIN_BUFFER_DIMENSION, MIN_CHANNEL_COUNT - 1));
    EXPECT_FALSE(within_display_limits(
        MAX_BUFFER_DIMENSION + 1, MAX_BUFFER_DIMENSION, MAX_CHANNEL_COUNT));
    EXPECT_FALSE(within_display_limits(
        MAX_BUFFER_DIMENSION, MAX_BUFFER_DIMENSION + 1, MAX_CHANNEL_COUNT));
    EXPECT_FALSE(within_display_limits(
        MAX_BUFFER_DIMENSION, MAX_BUFFER_DIMENSION, MAX_CHANNEL_COUNT + 1));
}

TEST(RawDataDecodeTest,
     PaddedPayloadSizeReturnsExactByteCountForKnownGeometry) {
    // width=4, height=3, channels=2, stride=6, FLOAT32:
    // stride*height*channels*type_size = 6*3*2*4 = 144 bytes.
    const auto size = padded_payload_size(4, 3, 2, 6, BufferType::FLOAT32);
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(*size, 144u);
}

TEST(RawDataDecodeTest, PaddedPayloadSizeRejectsNonPositiveWidth) {
    EXPECT_FALSE(
        padded_payload_size(0, 2, 1, 2, BufferType::UNSIGNED_BYTE).has_value());
    EXPECT_FALSE(padded_payload_size(-1, 2, 1, 2, BufferType::UNSIGNED_BYTE)
                     .has_value());
}

TEST(RawDataDecodeTest, PaddedPayloadSizeRejectsNonPositiveHeight) {
    EXPECT_FALSE(
        padded_payload_size(2, 0, 1, 2, BufferType::UNSIGNED_BYTE).has_value());
    EXPECT_FALSE(padded_payload_size(2, -1, 1, 2, BufferType::UNSIGNED_BYTE)
                     .has_value());
}

TEST(RawDataDecodeTest, PaddedPayloadSizeRejectsNonPositiveChannels) {
    EXPECT_FALSE(
        padded_payload_size(2, 2, 0, 2, BufferType::UNSIGNED_BYTE).has_value());
    EXPECT_FALSE(padded_payload_size(2, 2, -1, 2, BufferType::UNSIGNED_BYTE)
                     .has_value());
}

TEST(RawDataDecodeTest, PaddedPayloadSizeRejectsStrideBelowWidth) {
    EXPECT_FALSE(
        padded_payload_size(4, 2, 1, 3, BufferType::UNSIGNED_BYTE).has_value());
}

TEST(RawDataDecodeTest, PaddedPayloadSizeDoesNotOverflowOnHostileGeometry) {
    // stride*height*channels*type_size overflows 64 bits, so each factor
    // must be checked before it is applied, not after it has wrapped.
    constexpr int width = 1000000;
    constexpr int height = 1000000;
    constexpr int stride = 1000000000;
    constexpr int channels = 1000000000;
    EXPECT_FALSE(padded_payload_size(
                     width, height, channels, stride, BufferType::FLOAT64)
                     .has_value());
}

TEST(RawDataDecodeTest, PaddedPayloadSizeChecksTheFinalFactorToo) {
    // The case above trips the guard on `channels`; here only the final
    // multiply by sizeof(double) overflows, so the last factor is exercised.
    constexpr int max_int = (std::numeric_limits<int>::max)();
    EXPECT_FALSE(
        padded_payload_size(1, max_int, 1, max_int, BufferType::FLOAT64)
            .has_value());
    // Same geometry at one byte per element is representable, so the
    // rejection above is the overflow guard and not the geometry itself.
    EXPECT_TRUE(
        padded_payload_size(1, max_int, 1, max_int, BufferType::UNSIGNED_BYTE)
            .has_value());
}
