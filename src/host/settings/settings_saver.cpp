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

#include "host/settings/settings_saver.h"

#include <utility>

namespace oid::host {

SettingsSaver::SettingsSaver(AppSettings initial,
                             SaveSink sink,
                             double debounce_s)
    : save_{std::move(sink)}, current_{std::move(initial)},
      debounce_s_{debounce_s} {}

void SettingsSaver::update(const AppSettings& live, double now_s) {
    if (!(live == current_)) {
        current_ = live;
        dirty_ = true;
    }
    if (dirty_ && now_s - last_save_s_ >= debounce_s_) {
        save_(current_);
        dirty_ = false;
        last_save_s_ = now_s;
    }
}

void SettingsSaver::flush() {
    if (dirty_) {
        save_(current_);
        dirty_ = false;
    }
}

} // namespace oid::host
