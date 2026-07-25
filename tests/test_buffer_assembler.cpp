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

// Regression test: `stride` is row stride in elements, not bytes. For a
// multi-byte element the geometry must come from the payload (total bytes /
// height), or rows silently go missing while size checks still pass.
TEST(BufferAssemblerTests, AssemblesMultiByteElementBufferFromPayloadGeometry) {
    constexpr int width = 4;
    constexpr int height = 6;
    constexpr int stride = width; // elements per row, deliberately != bytes/row
    constexpr std::size_t bytes_per_row = width * sizeof(double);
    constexpr std::size_t total = bytes_per_row * height;
    BufferAssembler a;
    a.begin(make_begin("buf", width, height, stride, total));

    const auto full = iota_bytes(total);

    // Row-strip sizes chosen so a stride-in-elements assembler (4 bytes/row)
    // would reject these chunks outright (wrong expected size).
    ASSERT_TRUE(
        a.chunk("buf", 0, 2, std::span{full.data(), 2 * bytes_per_row}));
    ASSERT_TRUE(
        a.chunk("buf",
                2,
                4,
                std::span{full.data() + 2 * bytes_per_row, 4 * bytes_per_row}));

    const auto result = a.end("buf");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->bytes, full);
}

TEST(BufferAssemblerTests, RejectsWrongSizedChunkForMultiByteElementBuffer) {
    constexpr int width = 4;
    constexpr int height = 6;
    constexpr int stride = width;
    constexpr std::size_t bytes_per_row = width * sizeof(double);
    constexpr std::size_t total = bytes_per_row * height;
    BufferAssembler a;
    a.begin(make_begin("buf", width, height, stride, total));
    // One byte short of a single row.
    const auto strip = iota_bytes(bytes_per_row - 1);
    EXPECT_FALSE(a.chunk("buf", 0, 1, std::span{strip.data(), strip.size()}));
}

TEST(BufferAssemblerTests, RejectsNegativeHeight) {
    constexpr int stride = 8;
    constexpr int height = -4;
    constexpr std::size_t total = 32;
    BufferAssembler a;
    a.begin(make_begin("buf", 8, height, stride, total));
    const auto strip = iota_bytes(8);
    // begin() must not have started a transfer for "buf".
    EXPECT_FALSE(a.chunk("buf", 0, 1, std::span{strip.data(), strip.size()}));
    EXPECT_FALSE(a.end("buf").has_value());
}

TEST(BufferAssemblerTests, RejectsHeightExceedingTotalByteSize) {
    constexpr int stride = 8;
    constexpr int height = 100;
    constexpr std::size_t total = 32; // far fewer bytes than height rows
    BufferAssembler a;
    a.begin(make_begin("buf", 8, height, stride, total));
    const auto strip = iota_bytes(8);
    EXPECT_FALSE(a.chunk("buf", 0, 1, std::span{strip.data(), strip.size()}));
    EXPECT_FALSE(a.end("buf").has_value());
}

TEST(BufferAssemblerTests, RejectsTotalByteSizeNotDivisibleByHeight) {
    constexpr int stride = 8;
    constexpr int height = 5;
    constexpr std::size_t total = 33; // 33 % 5 != 0
    BufferAssembler a;
    a.begin(make_begin("buf", 8, height, stride, total));
    const auto strip = iota_bytes(8);
    EXPECT_FALSE(a.chunk("buf", 0, 1, std::span{strip.data(), strip.size()}));
    EXPECT_FALSE(a.end("buf").has_value());
}

TEST(BufferAssemblerTests, EmptyChunkDoesNotCountAsCoverage) {
    constexpr int stride = 4;
    constexpr int height = 2;
    constexpr std::size_t total = stride * height;
    BufferAssembler a;
    a.begin(make_begin("buf", 4, height, stride, total));
    // A zero-row, zero-byte chunk is a harmless no-op, but must not be
    // mistaken for a completed transfer.
    EXPECT_TRUE(a.chunk("buf", 0, 0, std::span<const std::byte>{}));
    EXPECT_FALSE(a.end("buf").has_value());
}

TEST(BufferAssemblerTests, RejectsRowRangeExceedingHeight) {
    constexpr int stride = 8;
    constexpr int height = 4;
    constexpr std::size_t total = stride * height;
    BufferAssembler a;
    a.begin(make_begin("buf", 8, height, stride, total));
    const auto strip = iota_bytes(2 * stride);
    // rows [3,5) - row 4 does not exist for height=4.
    EXPECT_FALSE(a.chunk("buf", 3, 2, std::span{strip.data(), strip.size()}));
}

TEST(BufferAssemblerTests, RejectsRowOffsetThatWouldWrapByteMathAroundSizeT) {
    constexpr int stride = 8;
    constexpr int height = 4;
    constexpr std::size_t total = stride * height; // bytes_per_row == 8
    BufferAssembler a;
    a.begin(make_begin("buf", 8, height, stride, total));
    const auto strip = iota_bytes(2 * stride);
    // row_offset * bytes_per_row == 2^61 * 8 == 2^64, which wraps to 0 in
    // size_t arithmetic; an unchecked implementation would compute a
    // deceptively in-bounds offset for a row far past `height`.
    constexpr std::size_t wrapping_offset = std::size_t{1} << 61;
    EXPECT_FALSE(a.chunk(
        "buf", wrapping_offset, 2, std::span{strip.data(), strip.size()}));
}

TEST(BufferAssemblerTests, EndFailsWhenAStripIsMissing) {
    constexpr int stride = 8;
    constexpr int height = 4;
    constexpr std::size_t total = stride * height;
    BufferAssembler a;
    a.begin(make_begin("buf", 8, height, stride, total));
    const auto full = iota_bytes(total);
    // Only rows [0,2) ever arrive; rows [2,4) are missing.
    ASSERT_TRUE(
        a.chunk("buf",
                0,
                2,
                std::span{full.data(), 2 * static_cast<std::size_t>(stride)}));
    EXPECT_FALSE(a.end("buf").has_value());
}

TEST(BufferAssemblerTests, AssemblesCorrectlyWhenChunksArriveOutOfOrder) {
    constexpr int stride = 8;
    constexpr int height = 4;
    constexpr std::size_t total = stride * height;
    BufferAssembler a;
    a.begin(make_begin("buf", 8, height, stride, total));
    const auto full = iota_bytes(total);

    // Second half arrives first.
    ASSERT_TRUE(
        a.chunk("buf",
                2,
                2,
                std::span{full.data() + 2 * static_cast<std::size_t>(stride),
                          2 * static_cast<std::size_t>(stride)}));
    // Then the first half.
    ASSERT_TRUE(
        a.chunk("buf",
                0,
                2,
                std::span{full.data(), 2 * static_cast<std::size_t>(stride)}));

    const auto result = a.end("buf");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->bytes, full);
}

TEST(BufferAssemblerTests, InterleavedBuffersDoNotCorruptEachOther) {
    constexpr int stride = 4;
    constexpr int height = 1;
    constexpr std::size_t total = stride * height;
    BufferAssembler a;
    a.begin(make_begin("a", 4, height, stride, total));
    a.begin(make_begin("b", 4, height, stride, total));
    const std::vector da{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const std::vector db{
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

TEST(BufferAssemblerTests, RejectedBeginDropsATransferAlreadyInFlight) {
    // A rejected begin must leave nothing behind: otherwise the earlier
    // transfer stays live under the same name and keeps accepting chunks
    // against its own geometry, which is not the geometry being sent.
    constexpr int stride = 4;
    constexpr std::size_t total = 8;
    BufferAssembler a;
    a.begin(make_begin("x", 4, 2, stride, total));

    a.begin(make_begin("x", 4, -1, stride, total));

    const std::vector row{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    EXPECT_FALSE(a.chunk("x", 0, 1, std::span{row.data(), row.size()}));
    EXPECT_FALSE(a.end("x").has_value());
}
