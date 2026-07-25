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

#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include "visualization/buffer_span_fits.h"

using namespace oid;

// A FLOAT64 record carries float32 bytes by the time it reaches the renderer:
// make_buffer_record() narrows it and leaves the type tag alone. Sizing it at
// type_size()'s eight bytes would reject every valid float64 buffer.
TEST(DisplayElementSize, Float64IsFourBytesOnceNarrowed) {
    EXPECT_EQ(display_element_size(BufferType::FLOAT64), sizeof(float));
    EXPECT_EQ(display_element_size(BufferType::FLOAT32), sizeof(float));
    EXPECT_EQ(display_element_size(BufferType::INT32), sizeof(float));
    EXPECT_EQ(display_element_size(BufferType::SHORT), sizeof(short));
    EXPECT_EQ(display_element_size(BufferType::UNSIGNED_SHORT), sizeof(short));
    EXPECT_EQ(display_element_size(BufferType::UNSIGNED_BYTE), 1u);
}

TEST(BufferSpanFits, AcceptsAnExactlySizedPackedBuffer) {
    // 4x2 rgb8, no padding: 4*2*3 = 24 bytes.
    EXPECT_TRUE(buffer_span_fits(4, 2, 3, 4, 1, 24));
}

TEST(BufferSpanFits, RejectsOneByteShort) {
    EXPECT_FALSE(buffer_span_fits(4, 2, 3, 4, 1, 23));
}

// The bound the renderer actually needs: the final row is only indexed out to
// `width`, so trailing row padding on it need not be present.
TEST(BufferSpanFits, AcceptsATrimmedFinalRow) {
    // width 3, step 5, height 2, 1 channel: last addressed pixel is
    // (1*5 + 3) = 8 pixels, so 8 bytes suffice even though a fully padded
    // buffer would be 10.
    EXPECT_TRUE(buffer_span_fits(3, 2, 1, 5, 1, 8));
    EXPECT_FALSE(buffer_span_fits(3, 2, 1, 5, 1, 7));
}

TEST(BufferSpanFits, RejectsNonsenseGeometry) {
    EXPECT_FALSE(buffer_span_fits(0, 2, 1, 4, 1, 1024));
    EXPECT_FALSE(buffer_span_fits(4, 0, 1, 4, 1, 1024));
    EXPECT_FALSE(buffer_span_fits(4, 2, 0, 4, 1, 1024));
    EXPECT_FALSE(buffer_span_fits(4, 2, 1, 3, 1, 1024)); // step < width
    EXPECT_FALSE(buffer_span_fits(4, 2, 1, 4, 0, 1024)); // zero element size
}

// The out-of-bounds read this guard exists to stop: a buffer claiming a huge
// step but carrying only a row's worth of bytes.
TEST(BufferSpanFits, RejectsTheHugeStepUnderRead) {
    EXPECT_FALSE(buffer_span_fits(4, 2, 1, 100000, sizeof(float), 16));
}

// Hostile geometry whose byte requirement overflows a 64-bit product must be
// refused rather than wrapping to a small number and being accepted.
TEST(BufferSpanFits, DoesNotOverflowOnHostileGeometry) {
    constexpr int huge = (std::numeric_limits<int>::max)();
    EXPECT_FALSE(buffer_span_fits(huge, huge, 4, huge, sizeof(float), 1024));
}

// Guards the 32-bit build specifically. (height - 1) * step here is exactly
// 2^32, which wraps to zero in a 32-bit size_t and would make these four bytes
// look sufficient for the whole geometry. The arithmetic is done in 64 bits so
// the real requirement survives; this assertion is trivially true on a 64-bit
// host and is the only thing standing behind wasm, where it is not.
TEST(BufferSpanFits, DoesNotWrapWhereSizeTIsThirtyTwoBits) {
    constexpr int step = 65536;
    constexpr int height = 65537; // (height - 1) * step == 2^32
    EXPECT_FALSE(buffer_span_fits(4, height, 1, step, 1, 4));
    // The same geometry with genuinely sufficient bytes is still accepted, so
    // the rejection above is the bound and not a blanket refusal.
    constexpr std::uint64_t needed =
        (static_cast<std::uint64_t>(height) - 1) * step + 4;
    EXPECT_TRUE(buffer_span_fits(4, height, 1, step, 1, needed));
}

// A float64 buffer sized as the renderer sees it (four bytes per element) is
// accepted; sizing it at eight would have refused it.
TEST(BufferSpanFits, AcceptsAFloat64BufferAtItsNarrowedSize) {
    constexpr int width = 1024;
    constexpr int height = 1025;
    constexpr auto elem = display_element_size(BufferType::FLOAT64);
    EXPECT_EQ(elem, 4u);
    EXPECT_TRUE(
        buffer_span_fits(width,
                         height,
                         1,
                         width,
                         elem,
                         static_cast<std::size_t>(width) * height * elem));
    EXPECT_FALSE(
        buffer_span_fits(width,
                         height,
                         1,
                         width,
                         elem,
                         static_cast<std::size_t>(width) * height * elem - 1));
}
