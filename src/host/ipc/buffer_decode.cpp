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

#include "host/ipc/buffer_decode.h"

#include <utility>

namespace oid::host {

BufferRecord make_buffer_record(BufferRecordParams params) {
    BufferRecord record;
    record.variable_name = std::move(params.variable_name);
    record.display_name = std::move(params.display_name);
    record.pixel_layout = std::move(params.pixel_layout);
    record.transpose = params.transpose;
    record.width = params.width;
    record.height = params.height;
    record.channels = params.channels;
    record.step = params.stride;
    record.type = params.type;

    if (params.type == oid::BufferType::FLOAT64) {
        record.bytes = oid::make_float_buffer_from_double(params.bytes);
    } else {
        record.bytes = std::move(params.bytes);
    }

    return record;
}

} // namespace oid::host
