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

#include "buffer_assembler.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace oid {

void BufferAssembler::begin(BeginParams params) {
    // A rejected begin must also drop any transfer already in flight under
    // this name, or its chunks would keep landing in the previous
    // allocation, under the previous geometry.
    auto name = params.variable_name;
    // Validate while height is still `int`, before any unsigned cast can
    // turn a negative value into a huge row count.
    if (params.height <= 0) {
        in_progress_.erase(name);
        return;
    }
    const auto height = static_cast<std::size_t>(params.height);
    const auto total = params.total_byte_size;
    // The allocation must divide evenly into rows, else bytes-per-row is
    // meaningless and a malformed transfer would otherwise be accepted.
    if (total < height || total % height != 0) {
        in_progress_.erase(name);
        return;
    }

    InProgress entry{.params = std::move(params),
                     .bytes = std::vector<std::byte>(total, std::byte{}),
                     .rows_received = std::vector<bool>(height, false)};
    in_progress_.insert_or_assign(std::move(name), std::move(entry));
}

bool BufferAssembler::chunk(const std::string& name,
                            const std::size_t row_offset,
                            const std::size_t row_count,
                            const std::span<const std::byte> bytes) {
    const auto it = in_progress_.find(name);
    if (it == in_progress_.end()) {
        return false;
    }
    auto& [entryParams, entryBytes, rowsReceived] = it->second;
    const auto height = static_cast<std::size_t>(entryParams.height);

    // Checked bound: row_offset > height rules out wraparound below.
    if (row_offset > height || row_count > height - row_offset) {
        return false;
    }

    // `stride` is row stride in elements, not bytes; derive real
    // bytes-per-row from the allocation so multi-channel / multi-byte
    // element buffers assemble correctly.
    const auto bytes_per_row = entryBytes.size() / height;
    const auto offset = row_offset * bytes_per_row;
    if (const auto expected = row_count * bytes_per_row;
        bytes.size() != expected || offset + bytes.size() > entryBytes.size()) {
        return false;
    }
    std::memcpy(entryBytes.data() + offset, bytes.data(), bytes.size());
    std::fill_n(rowsReceived.begin() + static_cast<std::ptrdiff_t>(row_offset),
                row_count,
                true);
    return true;
}

std::optional<AssembledBuffer> BufferAssembler::end(const std::string& name) {
    const auto it = in_progress_.find(name);
    if (it == in_progress_.end()) {
        return std::nullopt;
    }
    auto& [params, bytes, rowsReceived] = it->second;
    // Refuse a partial transfer rather than hand back a zero-filled buffer.
    if (!std::ranges::all_of(rowsReceived, [](const bool r) { return r; })) {
        in_progress_.erase(it);
        return std::nullopt;
    }
    AssembledBuffer out{.variable_name = std::move(params.variable_name),
                        .display_name = std::move(params.display_name),
                        .pixel_layout = std::move(params.pixel_layout),
                        .transpose = params.transpose,
                        .width = params.width,
                        .height = params.height,
                        .channels = params.channels,
                        .stride = params.stride,
                        .type = params.type,
                        .bytes = std::move(bytes)};
    in_progress_.erase(it);
    return out;
}

} // namespace oid
