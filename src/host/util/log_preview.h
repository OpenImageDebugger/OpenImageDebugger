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

// The bound for a variable name echoed into a diagnostic, one policy for
// every diagnostic that echoes one: wider than log_preview()'s default,
// since names are legitimately long where layouts are not, and a truncated
// name still has to be findable in the debuggee.
inline constexpr std::size_t NAME_PREVIEW_CHARS = 64;

// Renders an untrusted string safe to interpolate into one log line.
//
// Two hazards, both closed here rather than at each call site: the value may
// be arbitrarily long (the wire allows strings far larger than anything a
// diagnostic should echo, so a preview plus the byte count replaces the
// tail), and it may carry control characters (a newline forges an extra log
// line, a terminal escape repaints the one it is on, so both land as visible
// escapes instead). The bound is applied to the source before escaping, so
// the escaped output stays within a small constant factor of `max_chars`.
[[nodiscard]] inline std::string log_preview(const std::string_view value,
                                             const std::size_t max_chars = 16) {
    const bool truncated = value.size() > max_chars;
    const std::string_view shown =
        truncated ? value.substr(0, max_chars) : value;
    std::string out;
    out.reserve(shown.size() + 16);
    for (std::size_t i = 0; i < shown.size(); ++i) {
        const char c = shown[i];
        switch (c) {
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            const auto byte = static_cast<unsigned char>(c);
            // U+0080..U+009F (the C1 controls, CSI among them) arrive as
            // the UTF-8 pairs 0xc2 0x80..0x9f: decoded, they steer a
            // Unicode-aware consumer the way a C0 byte steers a plain one,
            // and no scan of single bytes below 0x20 ever sees them. Only
            // the exact pairs are rewritten: lone 0x80..0x9f bytes are the
            // continuation bytes ordinary non-ASCII text is made of, and
            // escaping those would mangle every such name.
            if (byte == 0xc2 && i + 1 < shown.size()) {
                if (const auto next = static_cast<unsigned char>(shown[i + 1]);
                    next >= 0x80 && next <= 0x9f) {
                    out += std::format("\\u{:04x}", next);
                    ++i;
                    continue;
                }
            }
            if (byte < 0x20 || byte == 0x7f) {
                out += std::format("\\x{:02x}", byte);
            } else {
                out += c;
            }
        }
    }
    if (truncated) {
        out += std::format("... ({} bytes)", value.size());
    }
    return out;
}

} // namespace oid::host

#endif // HOST_UTIL_LOG_PREVIEW_H_
