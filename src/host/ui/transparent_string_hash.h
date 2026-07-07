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

#ifndef HOST_UI_TRANSPARENT_STRING_HASH_H_
#define HOST_UI_TRANSPARENT_STRING_HASH_H_

#include <cstddef>
#include <functional>
#include <string_view>

namespace oid::host {

// Transparent hash for std::string-keyed unordered containers: paired with
// std::equal_to<>, it enables heterogeneous lookup (std::string_view or
// const char*) without constructing a temporary std::string.
struct TransparentStringHash {
    using is_transparent = void;

    [[nodiscard]] std::size_t
    operator()(const std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
};

} // namespace oid::host

#endif // HOST_UI_TRANSPARENT_STRING_HASH_H_
