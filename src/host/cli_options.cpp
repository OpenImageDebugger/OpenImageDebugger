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

#include "host/cli_options.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <system_error>

namespace oid::host {

namespace {

// Null-safe although an in-range argv entry cannot be null: routing every
// alias through here keeps std::strcmp off a pointer no analyzer can prove.
bool matches(const char* arg, std::initializer_list<const char*> names) {
    return arg != nullptr &&
           std::ranges::any_of(names, [arg](const char* name) {
               return std::strcmp(arg, name) == 0;
           });
}

// The whole token must be numeric, representable and positive. std::from_chars
// rather than std::atoi: out-of-range input is rejected instead of being UB.
std::optional<int> parse_positive_int(const char* text) {
    if (text == nullptr) {
        return std::nullopt;
    }
    // string_view derives the end from the null-checked pointer without a raw
    // strlen, keeping unbounded C-string functions off argv.
    const std::string_view sv{text};
    int value = 0;
    const auto [ptr, ec] =
        std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec == std::errc{} && ptr == sv.data() + sv.size() && value > 0) {
        return value;
    }
    return std::nullopt;
}

// Anything outside 1..65535 would narrow to the wrong unsigned short later, so
// it is rejected and the caller keeps the default.
std::optional<int> parse_port(const char* text) {
    if (const auto value = parse_positive_int(text);
        value.has_value() && *value <= 65535) {
        return value;
    }
    return std::nullopt;
}

} // namespace

CliOptions parse_cli(const int argc, const char* const* argv) {
    CliOptions options;

    // The advance is kept explicit rather than ++i inside argv[] so the loop
    // counter is never mutated mid-expression.
    int i = 1;
    while (i < argc) {
        const char* arg = argv[i];

        if (const char* value = i + 1 < argc ? argv[i + 1] : nullptr;
            matches(arg, {"-o", "--open"}) && value != nullptr) {
            options.open_files.emplace_back(value);
            i += 2;
        } else if (matches(arg, {"-h", "--hostname", "--host"}) &&
                   value != nullptr) {
            options.hostname = value;
            i += 2;
        } else if (matches(arg, {"-p", "--port"}) && value != nullptr) {
            if (const auto port = parse_port(value); port.has_value()) {
                options.port = *port;
            }
            i += 2;
        } else if (matches(arg, {"--agent-debugger-pid"}) && value != nullptr) {
            options.agent_debugger_pid = parse_positive_int(value);
            i += 2;
        } else {
            // Bare/unknown flags, and value-taking flags with no following
            // token, are ignored.
            ++i;
        }
    }

    return options;
}

} // namespace oid::host
