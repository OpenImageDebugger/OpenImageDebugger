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

#include <QCoreApplication>
#include <gtest/gtest.h>

#include "ui/messaging/session_state_codec.h"

using namespace oid;

namespace {
QCoreApplication& GetApplication() {
    static int argc = 1;
    static std::string app_name = "test";
    static char* argv[] = {app_name.data(), nullptr};
    static QCoreApplication application(argc, argv);
    return application;
}
} // namespace

TEST(SessionStateCodec, ParsesPrefs) {
    GetApplication();
    const auto json = QByteArray(
        R"({"rendering":{"framerate":30},"ui":{"contrastEnabled":true,)"
        R"("listPosition":"bottom","colorspace":"bgra"}})");
    SessionStateFields fields;
    ASSERT_TRUE(parse_session_state_json(json, fields));
    ASSERT_TRUE(fields.framerate.has_value());
    EXPECT_DOUBLE_EQ(*fields.framerate, 30.0);
    ASSERT_TRUE(fields.contrast_enabled.has_value());
    EXPECT_TRUE(*fields.contrast_enabled);
    ASSERT_TRUE(fields.list_position.has_value());
    EXPECT_EQ(*fields.list_position, QStringLiteral("bottom"));
    ASSERT_TRUE(fields.colorspace.has_value());
    EXPECT_EQ(*fields.colorspace, QStringLiteral("bgra"));
}

TEST(SessionStateCodec, SerializeIsPrefsOnly) {
    GetApplication();
    SettingsManager::DataCallbacks callbacks;
    callbacks.getRenderFramerate = [] { return 60.0; };
    callbacks.getDefaultExportSuffix = [] {
        return QStringLiteral("Image File (*.png)");
    };
    callbacks.getSplitterSizes = [] { return QList<int>{100, 200}; };
    callbacks.getMinMaxVisible = [] { return true; };
    callbacks.getContrastEnabled = [] { return false; };
    callbacks.getLinkViewsEnabled = [] { return false; };

    const SessionStateExtraInputs extra;
    const auto json = serialize_session_state_delta(callbacks, extra);
    EXPECT_FALSE(json.contains("held"));
    EXPECT_FALSE(json.contains("buffers"));
    EXPECT_FALSE(json.contains("previous"));
    EXPECT_TRUE(json.contains("framerate"));
}
