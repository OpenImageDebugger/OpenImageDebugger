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

#include "ipc/buffer_assembler.h"

#include <gtest/gtest.h>

using namespace oid;

namespace {

BufferAssembler::BeginParams make_begin(const std::string& name,
                                        const int width,
                                        const int height,
                                        const int stride,
                                        const std::size_t total) {
    return BufferAssembler::BeginParams{.variable_name = name,
                                        .display_name = name,
                                        .pixel_layout = "rgba",
                                        .transpose = false,
                                        .width = width,
                                        .height = height,
                                        .channels = 1,
                                        .stride = stride,
                                        .type = 0,
                                        .total_byte_size = total};
}

std::vector<std::byte> iota_bytes(const std::size_t n) {
    std::vector<std::byte> v(n);
    for (std::size_t i = 0; i < n; ++i) {
        v[i] = static_cast<std::byte>(i & 0xff);
    }
    return v;
}

} // namespace

TEST(BufferAssemblerTests, ReassemblesRowStripsIntoContiguousBuffer) {
    constexpr int stride = 8;
    constexpr int height = 4;
    constexpr std::size_t total = stride * height;
    BufferAssembler a;
    a.begin(make_begin("buf", 8, height, stride, total));

    const auto full = iota_bytes(total);

    // Feed chunk 0: first 2 rows
    ASSERT_TRUE(
        a.chunk("buf",
                0,
                2,
                std::span{full.data(), 2 * static_cast<std::size_t>(stride)}));

    // Feed chunk 1: next 2 rows
    ASSERT_TRUE(
        a.chunk("buf",
                2,
                2,
                std::span{full.data() + 2 * static_cast<std::size_t>(stride),
                          2 * static_cast<std::size_t>(stride)}));

    const auto result = a.end("buf");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->bytes, full);
    EXPECT_EQ(result->variable_name, "buf");
}

TEST(BufferAssemblerTests, RejectsChunkBeyondImageHeight) {
    constexpr int stride = 8;
    constexpr int height = 2;
    constexpr std::size_t total = stride * height;
    BufferAssembler a;
    a.begin(make_begin("buf", 8, height, stride, total));
    const auto strip = iota_bytes(2 * stride);
    // rows [1,3) do not fit in 2-row buffer.
    EXPECT_FALSE(a.chunk("buf", 1, 2, std::span{strip.data(), strip.size()}));
}

TEST(BufferAssemblerTests, RejectsWrongSizedChunk) {
    constexpr int stride = 8;
    constexpr int height = 2;
    constexpr std::size_t total = stride * height;
    BufferAssembler a;
    a.begin(make_begin("buf", 8, height, stride, total));
    const auto strip = iota_bytes(stride + 1); // not multiple stride
    EXPECT_FALSE(a.chunk("buf", 0, 1, std::span{strip.data(), strip.size()}));
}

TEST(BufferAssemblerTests, RejectsChunkAndEndForUnknownBuffer) {
    BufferAssembler a;
    const auto strip = iota_bytes(8);
    EXPECT_FALSE(
        a.chunk("missing", 0, 1, std::span{strip.data(), strip.size()}));
    EXPECT_FALSE(a.end("missing").has_value());
}

TEST(BufferAssemblerTests, InterleavedBuffersDoNotCorruptEachOther) {
    constexpr int stride = 4;
    constexpr int height = 1;
    constexpr std::size_t total = stride * height;
    BufferAssembler a;
    a.begin(make_begin("a", 4, height, stride, total));
    a.begin(make_begin("b", 4, height, stride, total));
    const std::vector<std::byte> da{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const std::vector<std::byte> db{
        std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}};
    ASSERT_TRUE(a.chunk("a", 0, 1, std::span{da.data(), da.size()}));
    ASSERT_TRUE(a.chunk("b", 0, 1, std::span{db.data(), db.size()}));

    const auto result_a = a.end("a");
    const auto result_b = a.end("b");
    ASSERT_TRUE(result_a.has_value());
    ASSERT_TRUE(result_b.has_value());
    EXPECT_EQ(result_a->bytes[0], std::byte{1});
    EXPECT_EQ(result_b->bytes[0], std::byte{5});
}
