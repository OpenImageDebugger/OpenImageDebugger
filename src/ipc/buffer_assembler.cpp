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

#include <cstring>
#include <utility>

namespace oid {

void BufferAssembler::begin(BeginParams params) {
    const auto total = params.total_byte_size;
    auto& entry = in_progress_[params.variable_name];
    entry.params = std::move(params);
    entry.bytes.assign(total, std::byte{});
}

bool BufferAssembler::chunk(const std::string& name,
                            const std::size_t row_offset,
                            const std::size_t row_count,
                            const std::span<const std::byte> bytes) {
    const auto it = in_progress_.find(name);
    if (it == in_progress_.end()) {
        return false;
    }
    auto& entry = it->second;
    const auto stride = static_cast<std::size_t>(entry.params.stride);
    const auto offset = row_offset * stride;
    if (const auto expected = row_count * stride;
        bytes.size() != expected ||
        offset + bytes.size() > entry.bytes.size()) {
        return false;
    }
    std::memcpy(entry.bytes.data() + offset, bytes.data(), bytes.size());
    return true;
}

std::optional<AssembledBuffer> BufferAssembler::end(const std::string& name) {
    const auto it = in_progress_.find(name);
    if (it == in_progress_.end()) {
        return std::nullopt;
    }
    auto& entry = it->second;
    AssembledBuffer out{.variable_name = std::move(entry.params.variable_name),
                        .display_name = std::move(entry.params.display_name),
                        .pixel_layout = std::move(entry.params.pixel_layout),
                        .transpose = entry.params.transpose,
                        .width = entry.params.width,
                        .height = entry.params.height,
                        .channels = entry.params.channels,
                        .stride = entry.params.stride,
                        .type = entry.params.type,
                        .bytes = std::move(entry.bytes)};
    in_progress_.erase(it);
    return out;
}

} // namespace oid
