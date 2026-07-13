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

#ifndef HOST_IO_EXPECTED_H_
#define HOST_IO_EXPECTED_H_

#include <optional>
#include <string>
#include <utility>

namespace oid {

// Error branch of Expected<T>, carrying a human-readable message. Any
// Expected<T> constructs a failure implicitly from an Unexpected, mirroring
// the value/error pairing of the C++23 standard library's expected facility
// (this project targets C++20, so it is reimplemented here).
struct Unexpected {
    std::string message;
};

[[nodiscard]] inline Unexpected make_error(std::string message) {
    return Unexpected{std::move(message)};
}

// Minimal value-or-error-message result type for the file-loading pipeline.
template <typename T> class Expected {
  public:
    Expected(T value) : value_{std::move(value)} {}
    Expected(Unexpected error) : error_{std::move(error.message)} {}

    [[nodiscard]] bool has_value() const {
        return value_.has_value();
    }
    explicit operator bool() const {
        return value_.has_value();
    }

    const T& operator*() const {
        return *value_;
    }
    T& operator*() {
        return *value_;
    }
    const T* operator->() const {
        return &*value_;
    }
    T* operator->() {
        return &*value_;
    }

    [[nodiscard]] const std::string& error() const {
        return error_;
    }

  private:
    std::optional<T> value_;
    std::string error_;
};

} // namespace oid

#endif // HOST_IO_EXPECTED_H_
