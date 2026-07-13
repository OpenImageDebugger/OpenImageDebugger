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

#ifndef HOST_IO_FILE_BUFFER_LOADER_H_
#define HOST_IO_FILE_BUFFER_LOADER_H_

#include <cstddef>
#include <span>
#include <string>

#include "host/io/expected.h"
#include "host/ui/buffer_model.h"

namespace oid::host {

// Cap on the file size the "open file" viewer will read into memory.
inline constexpr std::size_t MAX_OPEN_FILE_BYTES = 512ULL * 1024 * 1024;

// Decode already-read file bytes into a BufferRecord tagged LOCAL_FILE.
// Dispatches to the .npy decoder (on npy magic) or the stb image decoder.
[[nodiscard]] Expected<BufferRecord>
decode_file_bytes(std::span<const std::byte> bytes,
                  std::string variable_name,
                  std::string display_name);

// Reads a file from disk (rejecting files larger than max_bytes) and decodes
// it into a LOCAL_FILE BufferRecord. variable_name is the canonical path;
// display_name is the filename component.
[[nodiscard]] Expected<BufferRecord>
load_buffer_from_file(const std::string& path,
                      std::size_t max_bytes = MAX_OPEN_FILE_BYTES);

} // namespace oid::host

#endif // HOST_IO_FILE_BUFFER_LOADER_H_
