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

#ifndef HOST_SETTINGS_APP_SETTINGS_H_
#define HOST_SETTINGS_APP_SETTINGS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oid::host {

struct PreviousBuffer {
    std::string variable_name;
    std::int64_t expiry_epoch_s{};

    friend bool operator==(const PreviousBuffer&,
                           const PreviousBuffer&) = default;
};

struct AppSettings {
    int window_w{1024};
    int window_h{768};
    std::optional<int> window_x{};
    std::optional<int> window_y{};
    float left_pane_w{260.0f};
    bool contrast_enabled{true};
    bool link_views{false};
    std::vector<PreviousBuffer> previous_buffers{};
    std::string last_export_dir{};

    friend bool operator==(const AppSettings&, const AppSettings&) = default;
};

// Which parts of AppSettings this build is entitled to persist.
//
// Natively the viewer owns its window and its session, so it persists all of
// it to its own settings file. Embedded in a host, the window belongs to that
// host and the buffer list belongs to whatever restore the host runs, so a
// build that sends either one is telling somebody else about their own state
// -- and, in the case of the buffer list, being told it back as an
// instruction to plot those buffers again.
enum class SettingsScope { FULL, VIEWER_OWNED };

// `live` as this scope is entitled to persist it. Identity under FULL.
//
// Under VIEWER_OWNED the host-owned fields come back at their defaults, which
// is what keeps them out of SettingsSaver's comparison: the window size does
// not converge behind an embedded canvas, so a snapshot that still carried it
// would differ from its predecessor on nearly every frame and the saver would
// emit for the life of the session whether or not anyone touched a setting.
//
// Takes `live` by value so a caller with a temporary or a moved-from local
// can hand it over without a copy; both branches below return the parameter
// itself, which moves rather than copies.
[[nodiscard]] inline AppSettings settings_for_scope(AppSettings live,
                                                    const SettingsScope scope) {
    if (scope == SettingsScope::FULL) {
        return live;
    }

    const AppSettings defaults{};
    live.window_w = defaults.window_w;
    live.window_h = defaults.window_h;
    live.window_x = defaults.window_x;
    live.window_y = defaults.window_y;
    live.previous_buffers = defaults.previous_buffers;
    return live;
}

} // namespace oid::host

#endif // HOST_SETTINGS_APP_SETTINGS_H_
