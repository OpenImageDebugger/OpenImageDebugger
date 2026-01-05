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
#include <vector>

#include "ipc/raw_data_decode.h"

using namespace oid;

namespace {
constexpr double TEST_PI = 3.14159;
constexpr double TEST_LARGE_VALUE = 1e10;
constexpr double TEST_NEGATIVE_VALUE = -42.5;
constexpr double TEST_VALUE_1 = 1.0;
constexpr double TEST_VALUE_2_5 = 2.5;
constexpr double TEST_VALUE_4 = 4.0;
constexpr double TEST_VALUE_5 = 5.0;
} // namespace

TEST(RawDataDecodeTest, TypeSizeUnsignedByte) {
  EXPECT_EQ(type_size(BufferType::UnsignedByte), sizeof(std::uint8_t));
}

TEST(RawDataDecodeTest, TypeSizeUnsignedShort) {
  EXPECT_EQ(type_size(BufferType::UnsignedShort), sizeof(std::int16_t));
}

TEST(RawDataDecodeTest, TypeSizeShort) {
  EXPECT_EQ(type_size(BufferType::Short), sizeof(std::int16_t));
}

TEST(RawDataDecodeTest, TypeSizeInt32) {
  EXPECT_EQ(type_size(BufferType::Int32), sizeof(std::int32_t));
}

TEST(RawDataDecodeTest, TypeSizeFloat32) {
  EXPECT_EQ(type_size(BufferType::Float32), sizeof(float));
}

TEST(RawDataDecodeTest, TypeSizeFloat64) {
  EXPECT_EQ(type_size(BufferType::Float64), sizeof(double));
}

TEST(RawDataDecodeTest, MakeFloatBufferFromDouble_Empty) {
  std::vector<std::uint8_t> empty;
  auto result = make_float_buffer_from_double(empty);
  EXPECT_TRUE(result.empty());
}

namespace {
void TestSingleDoubleValue(double value) {
  std::vector<std::uint8_t> double_buffer(sizeof(double));
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
  const std::vector<double> values = {TEST_VALUE_1, TEST_VALUE_2_5, TEST_PI,
                                      TEST_VALUE_4, TEST_VALUE_5};
  std::vector<std::uint8_t> double_buffer(values.size() * sizeof(double));
  std::memcpy(double_buffer.data(), values.data(), double_buffer.size());
  const auto float_buffer = make_float_buffer_from_double(double_buffer);

  EXPECT_EQ(float_buffer.size(), values.size() * sizeof(float));
  for (auto i = 0U; i < values.size(); ++i) {
    float result = 0.0f;
    std::memcpy(&result, float_buffer.data() + i * sizeof(float),
                sizeof(float));
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
