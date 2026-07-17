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

#ifndef HOST_AGENT_WIRE_BUFFER_TYPE_H_
#define HOST_AGENT_WIRE_BUFFER_TYPE_H_

#include "ipc/raw_data_decode.h"

namespace oid::host::agent {

// Maps an in-memory buffer element type to the type actually served on the
// wire. FLOAT64 payloads are narrowed to float32 on ingest (make_buffer_record
// in buffer_decode.cpp), so the agent must report FLOAT32 for them and every
// other type as-is. Reporting FLOAT64 would make oid-mcp size the payload at 8
// bytes/element (rejecting every double buffer) and make the get_buffer
// pre-copy cap over-estimate. Kept as a free function so it is testable without
// the render-thread-bound NativeViewModel.
constexpr oid::BufferType wire_buffer_type(const oid::BufferType type) {
    return type == oid::BufferType::FLOAT64 ? oid::BufferType::FLOAT32 : type;
}

} // namespace oid::host::agent

#endif // HOST_AGENT_WIRE_BUFFER_TYPE_H_
