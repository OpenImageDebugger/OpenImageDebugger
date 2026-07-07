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

#include "host/ui/symbol_filter.h"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string_view>

namespace oid::host {

namespace {

char ascii_tolower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool ci_char_equal(char a, char b) {
    return ascii_tolower(a) == ascii_tolower(b);
}

bool contains_ci(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }

    return !std::ranges::search(haystack, needle, ci_char_equal).empty();
}

bool ci_less(std::string_view a, std::string_view b) {
    return std::ranges::lexicographical_compare(a, b, [](char lhs, char rhs) {
        return ascii_tolower(lhs) < ascii_tolower(rhs);
    });
}

} // namespace

std::vector<std::string>
filter_symbols(const std::vector<std::string>& candidates,
               std::string_view query) {
    std::vector<std::string> result;
    result.reserve(candidates.size());

    for (const auto& candidate : candidates) {
        if (contains_ci(candidate, query)) {
            result.push_back(candidate);
        }
    }

    std::ranges::stable_sort(result, ci_less);

    return result;
}

int symbol_completion_nav(int current, int match_count, bool up, bool down) {
    if (match_count <= 0) {
        return 0;
    }
    int idx = current;
    if (down && !up) {
        idx += 1;
    } else if (up && !down) {
        idx -= 1;
    }
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= match_count - 1) {
        idx = match_count - 1;
    }
    return idx;
}

} // namespace oid::host
