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

// What this build may persist. Embedded, the window and the buffer list belong
// to the host, and that buffer list comes back as an instruction to replot.
enum class SettingsScope { FULL, VIEWER_OWNED };

// `live` reduced to what `scope` may persist; identity under FULL. Host-owned
// fields return as defaults so SettingsSaver's diff cannot see an embedded
// canvas's never-converging window size and save on every frame.
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
