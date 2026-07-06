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

#ifndef IPC_BUFFER_ASSEMBLER_H_
#define IPC_BUFFER_ASSEMBLER_H_

#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace oid {

// Geometry + completed bytes for a buffer reassembled from row-strip chunks.
struct AssembledBuffer {
    std::string variable_name;
    std::string display_name;
    std::string pixel_layout;
    bool transpose{};
    int width{};
    int height{};
    int channels{};
    int stride{};
    int type{};
    std::vector<std::byte> bytes;
};

class BufferAssembler {
  public:
    struct BeginParams {
        std::string variable_name;
        std::string display_name;
        std::string pixel_layout;
        bool transpose{};
        int width{};
        int height{};
        int channels{};
        int stride{};
        int type{};
        std::size_t total_byte_size;
    };

    // Start transfer for buffer `name`.
    void begin(BeginParams params);

    // Append row-strip chunk. Returns false if name unknown, size mismatch, or
    // out of bounds.
    [[nodiscard]] bool chunk(const std::string& name,
                             std::size_t row_offset,
                             std::size_t row_count,
                             std::span<const std::byte> bytes);

    // Finish transfer, moving bytes out dropping entry. Returns
    // nullopt if name unknown.
    [[nodiscard]] std::optional<AssembledBuffer> end(const std::string& name);

  private:
    struct InProgress {
        BeginParams params;
        std::vector<std::byte> bytes;
    };
    std::map<std::string, InProgress, std::less<>> in_progress_{};
};

} // namespace oid

#endif // IPC_BUFFER_ASSEMBLER_H_
