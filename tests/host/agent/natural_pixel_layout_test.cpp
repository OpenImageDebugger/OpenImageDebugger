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

#include "host/agent/natural_pixel_layout.h"

#include <gtest/gtest.h>

namespace {

using oid::host::BufferKind;
using oid::host::BufferRecord;
using oid::host::agent::natural_pixel_layout;

// The unaffected case: set_channel(-1, _) ("all") restores a valid declared
// layout as declared, whatever the buffer currently renders with.
TEST(NaturalPixelLayout, ValidLayoutIsReturnedAsIs) {
    BufferRecord record;
    record.pixel_layout = "bgra";
    record.channels = 3;

    const auto result = natural_pixel_layout(record, "rgba");

    ASSERT_TRUE(result.layout.has_value());
    EXPECT_EQ(*result.layout, "bgra");
    EXPECT_FALSE(result.cleared_isolation);
}

// Post-ingest, a multi-channel DEBUGGER_SYMBOL layout is valid or defaulted,
// so an invalid one is residue: leave the current (non-isolation) layout.
TEST(NaturalPixelLayout,
     InvalidLayoutForMultiChannelDebuggerSymbolLeavesCurrentLayoutAlone) {
    BufferRecord record;
    record.pixel_layout = ""; // corrupted: e.g. a replot missing the hint
    record.channels = 3;
    record.kind = BufferKind::DEBUGGER_SYMBOL;

    const auto result = natural_pixel_layout(record, "bgra");

    EXPECT_FALSE(result.layout.has_value());
    EXPECT_FALSE(result.cleared_isolation);
}

// Same corruption, but four characters without the shader's alphabet: it
// clears the length floor and is still invalid, so it still hands off.
TEST(NaturalPixelLayout,
     InvalidCharactersForMultiChannelDebuggerSymbolLeavesCurrentLayoutAlone) {
    BufferRecord record;
    record.pixel_layout = "xyzq";
    record.channels = 4;
    record.kind = BufferKind::DEBUGGER_SYMBOL;

    const auto result = natural_pixel_layout(record, "bgra");

    EXPECT_FALSE(result.layout.has_value());
    EXPECT_FALSE(result.cleared_isolation);
}

// The one invalid layout that is not residue: a current isolation swizzle
// was set by this class's index arm, so "all" must restore the default.
TEST(NaturalPixelLayout,
     InvalidLayoutForMultiChannelDebuggerSymbolClearsIsolatedCurrentLayout) {
    BufferRecord record;
    record.pixel_layout = "";
    record.channels = 3;
    record.kind = BufferKind::DEBUGGER_SYMBOL;

    const auto result = natural_pixel_layout(record, "rrra");

    ASSERT_TRUE(result.layout.has_value());
    EXPECT_EQ(*result.layout, "rgba");
    EXPECT_TRUE(result.cleared_isolation);
}

// A LOCAL_FILE record never reaches resolve_pixel_layout (main.cpp upserts
// direct), so "" is convention; that check outranks the isolation one.
TEST(NaturalPixelLayout, LocalFileMultiChannelEmptyLayoutReturnsTheDefault) {
    BufferRecord record;
    record.pixel_layout = "";
    record.channels = 3;
    record.kind = BufferKind::LOCAL_FILE;

    const auto result = natural_pixel_layout(record, "rrra");

    ASSERT_TRUE(result.layout.has_value());
    EXPECT_EQ(*result.layout, "rgba");
    EXPECT_FALSE(result.cleared_isolation);
}

// Channel order is meaningless for one channel (shader_pixel_layout.h), so ""
// is convention even for DEBUGGER_SYMBOL, and outranks the isolation check.
TEST(NaturalPixelLayout, EmptyLayoutForSingleChannelReturnsTheDefault) {
    BufferRecord record;
    record.pixel_layout = "";
    record.channels = 1;
    record.kind = BufferKind::DEBUGGER_SYMBOL;

    const auto result = natural_pixel_layout(record, "ggga");

    ASSERT_TRUE(result.layout.has_value());
    EXPECT_EQ(*result.layout, "rgba");
    EXPECT_FALSE(result.cleared_isolation);
}

} // namespace
