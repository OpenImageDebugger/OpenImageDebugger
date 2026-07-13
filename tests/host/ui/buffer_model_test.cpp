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

#include "host/ui/buffer_model.h"

#include <gtest/gtest.h>

#include "ipc/raw_data_decode.h"

using oid::host::BufferKind;
using oid::host::make_default_mock_model;
using oid::host::MockBufferModel;
using oid::host::type_label;

TEST(MockBufferModel, HoldsDeterministicBuffers) {
    MockBufferModel m = make_default_mock_model();
    ASSERT_GE(m.size(), 2u);

    const auto& r = m.at(0);
    EXPECT_EQ(r.width * r.height * r.channels,
              static_cast<int>(r.bytes.size()));
    EXPECT_FALSE(r.variable_name.empty());
}

// Locally-opened buffers are tagged BufferKind::LOCAL_FILE so they can be
// excluded from GET_OBSERVED_SYMBOLS; every other record -- including these
// deterministic mock buffers -- defaults to DEBUGGER_SYMBOL.
TEST(MockBufferModel, DefaultRecordKindIsDebuggerSymbol) {
    MockBufferModel m = make_default_mock_model();
    ASSERT_GE(m.size(), 1u);
    EXPECT_EQ(m.at(0).kind, BufferKind::DEBUGGER_SYMBOL);
}

// step is "pixels per row" (see Buffer::configure / GL_UNPACK_ROW_LENGTH),
// NOT elements-per-row. For these contiguous buffers step must equal width,
// and bytes must cover width*height*channels*type_size(type). A wrong step
// silently passes Buffer's `step >= channels` check but makes GL read past
// the buffer once StageManager wires it through.
TEST(MockBufferModel, StepIsPixelsPerRowAndBytesAreTypeSized) {
    MockBufferModel m = make_default_mock_model();
    for (std::size_t i = 0; i < m.size(); ++i) {
        const auto& r = m.at(i);
        EXPECT_EQ(r.step, r.width) << "record " << i;
        const auto expected = static_cast<std::size_t>(r.width) *
                              static_cast<std::size_t>(r.height) *
                              static_cast<std::size_t>(r.channels) *
                              oid::type_size(r.type);
        EXPECT_EQ(r.bytes.size(), expected) << "record " << i;
    }
}

// MockBufferModel's buffers are fixed at construction and never re-plot, so
// revision_of() is permanently 0 for every slot; variable_name_of() must
// still expose each record's stable key for StageManager's name-keyed sync.
TEST(MockBufferModel, VariableNameAndRevisionOf) {
    MockBufferModel m = make_default_mock_model();
    for (std::size_t i = 0; i < m.size(); ++i) {
        EXPECT_EQ(m.variable_name_of(i), m.at(i).variable_name)
            << "record " << i;
        EXPECT_EQ(m.revision_of(i), 0u) << "record " << i;
    }
}

TEST(TypeLabel, FormatsTypeAndChannels) {
    EXPECT_EQ(type_label(oid::BufferType::UNSIGNED_BYTE, 3), "uint8x3");
    EXPECT_EQ(type_label(oid::BufferType::FLOAT32, 1), "float32x1");
}

// MockBufferModel stores BufferRecords behind unique_ptr specifically so
// erasing one record never moves the survivors: any oid::Stage holding a
// std::span into a surviving BufferRecord::bytes must keep pointing at the
// same address after an unrelated record is erased. This test only checks
// the model in isolation (no live Stage), but the address-stability it
// asserts is exactly the property Stage's span relies on.
TEST(MockBufferModel, EraseKeepsSurvivorsAddressStable) {
    MockBufferModel m = make_default_mock_model();
    ASSERT_GE(m.size(), 3u);
    const std::size_t original_size = m.size();

    // Capture BOTH the record's own address and its bytes pointer. The
    // record address is the discriminating property: with vector<BufferRecord>
    // storage, erase() move-assigns survivors into lower slots, so &at(i)
    // WOULD change (even though vector<byte> move happens to preserve
    // bytes.data() via pointer-steal). unique_ptr storage keeps the pointee
    // fixed, so both must be stable -- which is what a Stage's span relies on.
    const oid::host::BufferRecord* record1_addr = &m.at(1);
    const oid::host::BufferRecord* record2_addr = &m.at(2);
    const std::byte* record1_bytes = m.at(1).bytes.data();
    const std::byte* record2_bytes = m.at(2).bytes.data();

    m.erase(0);

    EXPECT_EQ(m.size(), original_size - 1);
    EXPECT_EQ(&m.at(0), record1_addr);
    EXPECT_EQ(&m.at(1), record2_addr);
    EXPECT_EQ(m.at(0).bytes.data(), record1_bytes);
    EXPECT_EQ(m.at(1).bytes.data(), record2_bytes);

    // Out-of-range erase is a documented no-op.
    const std::size_t size_before_noop = m.size();
    m.erase(m.size());
    EXPECT_EQ(m.size(), size_before_noop);
}
