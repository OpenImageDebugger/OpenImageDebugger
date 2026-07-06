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

#ifndef HOST_SETTINGS_SETTINGS_SAVER_H_
#define HOST_SETTINGS_SETTINGS_SAVER_H_

#include "host/settings/app_settings.h"

#include <functional>

namespace oid::host {

// Diffs a live AppSettings snapshot against the last-saved copy each frame
// and persists it (via the injected SaveSink) no more often than
// debounce_s, so rapid changes (e.g. dragging a splitter) don't trigger a
// disk write per frame.
//
// SaveSink decouples SettingsSaver from any particular persistence
// mechanism: native wires a lambda that writes through SettingsStore (see
// main.cpp); a non-native host can wire a lambda that sends the
// settings over the wire instead, with the diff+debounce logic unchanged
// either way.
class SettingsSaver {
  public:
    using SaveSink = std::function<void(const AppSettings&)>;

    SettingsSaver(AppSettings initial, SaveSink sink, double debounce_s = 0.75);

    // Diffs `live` into the held copy (marks dirty on change); if dirty and
    // the debounce interval has elapsed since the last save, saves and
    // clears dirty.
    void update(const AppSettings& live, double now_s);

    // Forces a save if dirty. Call on exit to flush a pending debounced
    // save.
    void flush();

  private:
    SaveSink save_;
    AppSettings current_;
    bool dirty_{false};
    double last_save_s_{-1.0e9};
    double debounce_s_;
};

} // namespace oid::host

#endif // HOST_SETTINGS_SETTINGS_SAVER_H_
