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

#ifndef HOST_IO_NPY_DECODE_H_
#define HOST_IO_NPY_DECODE_H_

#include <cstddef>
#include <span>
#include <vector>

#include "host/io/expected.h"
#include "ipc/raw_data_decode.h"

namespace oid {

struct NpyArray {
    BufferType type{BufferType::UNSIGNED_BYTE};
    int width{0};
    int height{0};
    int channels{1};
    int step{0};
    bool transpose{false};
    std::vector<std::byte> bytes;
};

// Decode a NumPy .npy byte buffer (v1/v2/v3, little-endian dtypes,
// C- or Fortran-order 2-D, C-order 3-D). Returns an error string on any
// unsupported or malformed input. FLOAT64 payloads are returned as raw
// double bytes; downstream conversion to float32 happens in the loader.
[[nodiscard]] Expected<NpyArray> decode_npy(std::span<const std::byte> data);

} // namespace oid

#endif // HOST_IO_NPY_DECODE_H_
