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

#include <gtest/gtest.h>

using namespace oid::host;

TEST(SettingsSaver, NoSaveWhenUnchanged) {
    int saves = 0;
    AppSettings s;
    SettingsSaver saver{s, [&saves](const AppSettings&) { ++saves; }, 0.75};
    saver.update(s, 0.0);
    saver.update(s, 100.0);
    EXPECT_EQ(saves, 0);
}

TEST(SettingsSaver, ChangeSavesOnlyAfterDebounce) {
    int saves = 0;
    AppSettings s;
    SettingsSaver saver{s, [&saves](const AppSettings&) { ++saves; }, 0.75};
    AppSettings changed = s;
    changed.left_pane_w = 300.0f;
    saver.update(
        changed,
        10.0); // dirty, but last_save is -1e9 -> elapsed -> saves immediately
    EXPECT_EQ(saves, 1);
    AppSettings changed2 = changed;
    changed2.window_w = 900;
    saver.update(changed2,
                 10.1); // dirty again, but <0.75s since last save -> no save
    EXPECT_EQ(saves, 1);
    saver.update(changed2, 11.0); // >=0.75s later -> saves
    EXPECT_EQ(saves, 2);
}

TEST(SettingsSaver, FlushForcesPendingSave) {
    int saves = 0;
    AppSettings s;
    SettingsSaver saver{s, [&saves](const AppSettings&) { ++saves; }, 0.75};
    AppSettings a = s;
    a.link_views = true;
    saver.update(a, 0.0);
    EXPECT_EQ(saves, 1); // immediate (first save)
    AppSettings b = a;
    b.contrast_enabled = false;
    saver.update(b, 0.1); // dirty, debounced -> no save
    EXPECT_EQ(saves, 1);
    saver.flush();
    EXPECT_EQ(saves, 2);
    saver.flush();
    EXPECT_EQ(saves, 2); // nothing pending -> no extra save
}
