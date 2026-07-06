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

#include "host/ui/ipc_buffer_model.h"

#include <string_view>

#include <gtest/gtest.h>

using namespace oid::host;

static BufferRecord
rec(std::string_view name, std::byte fill, std::size_t n = 4) {
    BufferRecord r;
    r.variable_name = name;
    r.display_name = name;
    r.pixel_layout = "rgba";
    r.width = 2;
    r.height = 2;
    r.channels = 1;
    r.step = 2;
    r.type = oid::BufferType::UnsignedByte;
    r.bytes.assign(n, fill);
    return r;
}

TEST(IpcBufferModel, UpsertAddsThenReplacesByName) {
    IpcBufferModel m;
    m.upsert(rec("a", std::byte{1})); // -> index 0
    m.upsert(rec("b", std::byte{2})); // -> index 1
    ASSERT_EQ(m.size(), 2u);
    ASSERT_EQ(m.variable_name_of(0), "a");
    const std::uint64_t rev_a0 = m.revision_of(0);
    // Re-plot "a": replaces in place (index 0 stays "a"), size unchanged,
    // bytes updated, that slot's revision advances.
    m.upsert(rec("a", std::byte{9}));
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.variable_name_of(0), "a");
    EXPECT_EQ(m.at(0).bytes.front(), std::byte{9});
    EXPECT_GT(m.revision_of(0), rev_a0);
}

TEST(IpcBufferModel, VariableNameAndRevisionOfTrackReplots) {
    IpcBufferModel m;
    m.upsert(rec("a", std::byte{1}));
    m.upsert(rec("b", std::byte{2}));
    EXPECT_EQ(m.variable_name_of(0), "a");
    EXPECT_EQ(m.variable_name_of(1), "b");

    const std::uint64_t rev_a_before = m.revision_of(0);
    const std::uint64_t rev_b_before = m.revision_of(1);

    // Re-plotting "a" only advances slot 0's revision; "b" is untouched.
    m.upsert(rec("a", std::byte{7}));
    EXPECT_GT(m.revision_of(0), rev_a_before);
    EXPECT_EQ(m.revision_of(1), rev_b_before);
}

TEST(IpcBufferModel, RemoveErasesByName) {
    IpcBufferModel m;
    m.upsert(rec("a", std::byte{1}));
    m.upsert(rec("b", std::byte{2}));
    const std::uint64_t before = m.revision();
    m.remove("a");
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.variable_name_of(0), "b");
    EXPECT_GT(m.revision(), before);
    m.remove("nope"); // absent -> no-op, no crash
    EXPECT_EQ(m.size(), 1u);
}
