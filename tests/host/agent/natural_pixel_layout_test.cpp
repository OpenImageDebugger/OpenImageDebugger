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
// ("all") should restore, exactly as declared.
TEST(NaturalPixelLayout, ValidLayoutIsReturnedAsIs) {
    BufferRecord record;
    record.pixel_layout = "bgra";
    record.channels = 3;

    const auto result = natural_pixel_layout(record);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "bgra");
}

// Defect: NativeViewModel::set_channel's mode == -1 arm used to stamp the
// documented default onto the live buffer unconditionally whenever the
// record's layout failed validation, even for a multi-channel buffer: the
// exact mechanism that turns a model-level corruption (see
// IpcClient::handle_plot_buffer_contents) into a permanent, wrong render
// (Buffer::set_pixel_layout() has no buff_tex_ guard of its own). This is
// corruption only for a DEBUGGER_SYMBOL record: post-fix, the IPC ingest
// path guarantees a multi-channel DEBUGGER_SYMBOL record is either valid or
// already defaulted (see IpcClient::resolve_pixel_layout), so an invalid one
// reaching here really is residue or corruption, and the caller must leave
// the live buffer's layout untouched instead of guessing.
TEST(NaturalPixelLayout,
     InvalidLayoutForMultiChannelDebuggerSymbolLeavesCurrentLayoutAlone) {
    BufferRecord record;
    record.pixel_layout = ""; // corrupted: e.g. a replot missing the hint
    record.channels = 3;
    record.kind = BufferKind::DEBUGGER_SYMBOL;

    EXPECT_FALSE(natural_pixel_layout(record).has_value());
}

// Same corruption, but the record's layout has the mechanical floor (four
// characters) without the shader's alphabet: still invalid, still hands off
// to the caller rather than guessing.
TEST(NaturalPixelLayout,
     InvalidCharactersForMultiChannelDebuggerSymbolLeavesCurrentLayoutAlone) {
    BufferRecord record;
    record.pixel_layout = "xyzq";
    record.channels = 4;
    record.kind = BufferKind::DEBUGGER_SYMBOL;

    EXPECT_FALSE(natural_pixel_layout(record).has_value());
}

// The other half of the discriminator: a LOCAL_FILE record's empty layout is
// never corruption, whatever its channel count. layout_for_channels()
// (file_buffer_loader.cpp) returns "" for any non-4-channel file (a plain
// 3-channel RGB image is the common case), and main.cpp's file-open path
// upserts straight into the same IpcBufferModel NativeViewModel reads,
// bypassing IpcClient::resolve_pixel_layout entirely: nothing ever wire-
// validates this record, so an empty layout here is the documented
// convention, not residue, and restoring DEFAULT_PIXEL_LAYOUT is exactly
// what such a buffer already renders with (shader_pixel_layout.h keeps a
// non-single-channel texture's declared layout, so this must be the buffer's
// live layout already, not a guess about what it should be).
TEST(NaturalPixelLayout, LocalFileMultiChannelEmptyLayoutReturnsTheDefault) {
    BufferRecord record;
    record.pixel_layout = "";
    record.channels = 3;
    record.kind = BufferKind::LOCAL_FILE;

    const auto result = natural_pixel_layout(record);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "rgba");
}

// The convention this must not disturb: a single-channel buffer's empty
// layout is not a corruption (channel order is meaningless for one channel,
// see shader_pixel_layout.h), so restoring the documented default here is not
// a guess: it is exactly what a single-channel buffer already renders with.
// This holds for a DEBUGGER_SYMBOL record too, not only a LOCAL_FILE one.
TEST(NaturalPixelLayout, EmptyLayoutForSingleChannelReturnsTheDefault) {
    BufferRecord record;
    record.pixel_layout = "";
    record.channels = 1;
    record.kind = BufferKind::DEBUGGER_SYMBOL;

    const auto result = natural_pixel_layout(record);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "rgba");
}

} // namespace
