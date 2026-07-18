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

#include "host/agent/wire_buffer_type.h"

#include <gtest/gtest.h>

namespace {

using oid::BufferType;
using enum BufferType;
using oid::host::agent::wire_buffer_type;

// The only automated guard on the FLOAT64->float32 narrowing the ingest path
// applies: NativeViewModel serves float32 bytes for a double buffer, so it must
// advertise FLOAT32, or oid-mcp sizes the payload at 8 bytes/element (rejecting
// every double buffer) and the get_buffer pre-copy cap over-estimates.
TEST(WireBufferType, Float64IsNarrowedToFloat32) {
    EXPECT_EQ(wire_buffer_type(FLOAT64), FLOAT32);
}

TEST(WireBufferType, EveryOtherTypeIsServedAsIs) {
    for (const BufferType type :
         {UNSIGNED_BYTE, UNSIGNED_SHORT, SHORT, INT32, FLOAT32}) {
        EXPECT_EQ(wire_buffer_type(type), type);
    }
}

// constexpr-evaluable so it can never silently regress to a runtime-only map.
static_assert(wire_buffer_type(FLOAT64) == FLOAT32);
static_assert(wire_buffer_type(UNSIGNED_BYTE) == UNSIGNED_BYTE);

} // namespace
