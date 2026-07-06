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

#ifndef HOST_SETTINGS_SESSION_BUFFERS_H_
#define HOST_SETTINGS_SESSION_BUFFERS_H_

#include "host/settings/app_settings.h"

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace oid::host {

// Recompute the persisted set: every currently-loaded buffer gets a fresh
// expiry (now + ttl); a prior entry NOT currently loaded is preserved only if
// it was never loaded this session (not in `seen`) AND not expired -- i.e.
// a user-deleted buffer (in `seen`, now gone) is dropped, a not-yet-reloaded
// buffer survives. Mirrors SettingsManager::persist_settings' merge.
std::vector<PreviousBuffer> merge_previous_buffers(
    const std::vector<PreviousBuffer>& prior,
    const std::vector<std::string>& current_loaded,
    const std::set<std::string, std::less<>>& seen_this_session,
    std::int64_t now_s,
    std::int64_t ttl_s = 86400);

} // namespace oid::host

#endif // HOST_SETTINGS_SESSION_BUFFERS_H_
