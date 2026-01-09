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

#include "message_exchange.h"

#include <utility>

namespace oid
{

MessageBlock::~MessageBlock() noexcept = default;

StringBlock::StringBlock(std::string value)
    : data_{std::move(value)}
{
}

size_t StringBlock::size() const noexcept
{
    return data_.size();
}

[[nodiscard]] const std::byte* StringBlock::data() const noexcept
{
    // Note: reinterpret_cast is necessary here for pointer conversion from
    // const char* (std::string::data()) to const std::byte*. std::bit_cast
    // cannot be used for pointer-to-pointer conversions, only for value
    // conversions of same-sized types. Using std::byte* for semantic clarity
    // that this is raw byte storage.
    return reinterpret_cast<const std::byte*>(data_.data());
}

} // namespace oid
