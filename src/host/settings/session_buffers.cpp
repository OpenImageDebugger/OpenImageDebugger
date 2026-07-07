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

#include "host/settings/session_buffers.h"

namespace oid::host {

std::vector<PreviousBuffer> merge_previous_buffers(
    const std::vector<PreviousBuffer>& prior,
    const std::vector<std::string>& current_loaded,
    const std::set<std::string, std::less<>>& seen_this_session,
    std::int64_t now_s,
    std::int64_t ttl_s) {
    std::vector<PreviousBuffer> out;
    std::set<std::string, std::less<>> loaded{current_loaded.begin(),
                                              current_loaded.end()};
    for (const auto& name : current_loaded) {
        out.emplace_back(name, now_s + ttl_s); // fresh expiry
    }
    for (const auto& b : prior) {
        if (loaded.contains(b.variable_name)) {
            continue; // already added fresh
        }
        if (seen_this_session.contains(b.variable_name)) {
            continue; // user-deleted this run
        }
        if (b.expiry_epoch_s <= now_s) {
            continue; // expired
        }
        out.push_back(b); // not-yet-reloaded, keep as-is
    }
    return out;
}

} // namespace oid::host
