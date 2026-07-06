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

#ifndef HOST_IPC_BUFFER_DECODE_H_
#define HOST_IPC_BUFFER_DECODE_H_

#include <cstddef>
#include <string>
#include <vector>

#include "host/ui/buffer_model.h"
#include "ipc/raw_data_decode.h"

namespace oid::host {

// Builds a BufferRecord from already-decoded wire fields. This is the
// single funnel IpcClient's single-shot (PlotBufferContents) and
// chunked (PlotBufferEnd) decode paths both call, mirroring the Qt
// window's MessageHandler::plot_buffer_from_fields exactly: fields are
// assigned straight across, `stride` becomes BufferRecord::step, and
// Float64 payloads are converted to float bytes via
// oid::make_float_buffer_from_double() (BufferRecord::type is left as
// Float64, unchanged, matching the Qt path).
BufferRecord make_buffer_record(std::string variable_name,
                                std::string display_name,
                                std::string pixel_layout,
                                bool transpose,
                                int width,
                                int height,
                                int channels,
                                int stride,
                                oid::BufferType type,
                                std::vector<std::byte> bytes);

} // namespace oid::host

#endif // HOST_IPC_BUFFER_DECODE_H_
