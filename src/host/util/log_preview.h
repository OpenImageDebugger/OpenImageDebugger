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

#ifndef HOST_UTIL_LOG_PREVIEW_H_
#define HOST_UTIL_LOG_PREVIEW_H_

#include <cstddef>
#include <format>
#include <string>
#include <string_view>

namespace oid::host {

// Bound for a variable name echoed into a diagnostic: wider than
// log_preview()'s default, since a truncated name must stay findable.
inline constexpr std::size_t NAME_PREVIEW_CHARS = 64;

namespace detail {

// Escapes the character at text[i], returning the source bytes consumed.
//
// C1 controls arrive as the UTF-8 pairs 0xc2 0x80..0x9f and steer a
// Unicode-aware consumer the way a C0 byte steers a plain one; lone
// 0x80..0x9f bytes are ordinary continuation bytes and pass through.
[[nodiscard]] inline std::size_t append_escaped(const std::string_view text,
                                                const std::size_t i,
                                                std::string& out) {
    const char c = text[i];
    if (c == '\n') {
        out += "\\n";
        return 1;
    }
    if (c == '\r') {
        out += "\\r";
        return 1;
    }
    if (c == '\t') {
        out += "\\t";
        return 1;
    }
    const auto byte = static_cast<unsigned char>(c);
    if (byte == 0xc2 && i + 1 < text.size()) {
        if (const auto next = static_cast<unsigned char>(text[i + 1]);
            next >= 0x80 && next <= 0x9f) {
            out += std::format("\\u{:04x}", next);
            return 2;
        }
    }
    if (byte < 0x20 || byte == 0x7f) {
        out += std::format("\\x{:02x}", byte);
        return 1;
    }
    out += c;
    return 1;
}

} // namespace detail

// Renders an untrusted string safe to interpolate into one log line: a
// newline would forge a second line and a terminal escape repaint this one.
// The bound applies before escaping, so output stays near `max_chars`.
[[nodiscard]] inline std::string log_preview(const std::string_view value,
                                             const std::size_t max_chars = 16) {
    const bool truncated = value.size() > max_chars;
    const std::string_view shown =
        truncated ? value.substr(0, max_chars) : value;
    std::string out;
    out.reserve(shown.size() + 16);
    std::size_t i = 0;
    while (i < shown.size()) {
        i += detail::append_escaped(shown, i, out);
    }
    if (truncated) {
        out += std::format("... ({} bytes)", value.size());
    }
    return out;
}

} // namespace oid::host

#endif // HOST_UTIL_LOG_PREVIEW_H_
