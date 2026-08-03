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

// The unaffected case: a valid declared layout is what set_channel(-1, _)
// ("all") should restore, exactly as declared, regardless of what the
// buffer currently renders with.
TEST(NaturalPixelLayout, ValidLayoutIsReturnedAsIs) {
    BufferRecord record;
    record.pixel_layout = "bgra";
    record.channels = 3;

    const auto result = natural_pixel_layout(record, "rgba");

    ASSERT_TRUE(result.layout.has_value());
    EXPECT_EQ(*result.layout, "bgra");
    EXPECT_FALSE(result.cleared_isolation);
}

// Defect: NativeViewModel::set_channel's mode == -1 arm used to stamp the
// documented default onto the live buffer unconditionally whenever the
// record's layout failed validation, even for a multi-channel buffer: the
// exact mechanism that turns a model-level corruption (see
// IpcClient::handle_plot_buffer_contents) into a permanent, wrong render
// (Buffer::set_pixel_layout() has no buff_tex_ guard of its own). This is
// corruption only for a DEBUGGER_SYMBOL record whose current layout is not
// itself an isolation swizzle (see the isolation-clearing test below for the
// other branch): post-fix, the IPC ingest path guarantees a multi-channel
// DEBUGGER_SYMBOL record is either valid or already defaulted (see
// IpcClient::resolve_pixel_layout), so an invalid one reaching here really
// is residue or corruption, and the caller must leave the live buffer's
// layout untouched instead of guessing. "bgra" is used as the current
// layout here (a valid, non-isolation layout the buffer might already be
// correctly rendering with) so this is unambiguously the leave-alone case,
// not the isolation-clearing one.
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

// Same corruption, but the record's layout has the mechanical floor (four
// characters) without the shader's alphabet: still invalid, still hands off
// to the caller rather than guessing. Current layout is again a valid,
// non-isolation layout, for the same reason as above.
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

// Defect, other half: exiting isolation must actually exit it. If the user
// previously isolated a channel (set_channel(name, index, _) installs one of
// ISOLATION_LAYOUTS: "rrra"/"ggga"/"bbba") and the record's layout is invalid
// for a multi-channel DEBUGGER_SYMBOL buffer, leaving the buffer's current
// (isolated) layout alone would silently keep rendering a single channel
// even though "all channels" was just requested and display_channel_mode
// resets to -1. This is the one case where an invalid layout is NOT residue
// to be left alone: the current layout being an isolation swizzle proves it
// was set by this same class's own index arm, not preserved from a valid
// declaration, so restoring the default is correct, not a guess, and it must
// happen loudly.
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

// The other half of the discriminator: a LOCAL_FILE record's empty layout is
// never corruption, whatever its channel count, even when the buffer
// currently holds an isolation swizzle: the LOCAL_FILE/channel-count check
// takes precedence over the isolation check, so this is a plain default, not
// a "cleared isolation" one (there is nothing for this record to lose by
// leaving isolation alone versus not; the default is simply always correct
// here). layout_for_channels() (file_buffer_loader.cpp) returns "" for any
// non-4-channel file (a plain 3-channel RGB image is the common case), and
// main.cpp's file-open path upserts straight into the same IpcBufferModel
// NativeViewModel reads, bypassing IpcClient::resolve_pixel_layout entirely:
// nothing ever wire-validates this record, so an empty layout here is the
// documented convention, not residue.
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

// The convention this must not disturb: a single-channel buffer's empty
// layout is not a corruption (channel order is meaningless for one channel,
// see shader_pixel_layout.h), so restoring the documented default here is not
// a guess: it is exactly what a single-channel buffer already renders with.
// This holds for a DEBUGGER_SYMBOL record too, not only a LOCAL_FILE one.
// The current layout is again an isolation swizzle here, to prove the
// single-channel check takes precedence over the isolation check the same
// way the LOCAL_FILE one does above.
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
