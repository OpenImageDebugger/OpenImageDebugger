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

#include "host/settings/config_path.h"
#include <gtest/gtest.h>
using namespace oid::host;

TEST(ConfigPath, ComposesPlatformPath) {
    const auto p = config_file_path_from_env(nullptr, "/home/u", nullptr);
    EXPECT_EQ(p.filename(), "imgui_settings.json");
    EXPECT_NE(p.string().find("OpenImageDebugger"), std::string::npos);
#if defined(__linux__)
    EXPECT_EQ(p,
              std::filesystem::path{
                  "/home/u/.config/OpenImageDebugger/imgui_settings.json"});
#elif defined(__APPLE__)
    EXPECT_EQ(
        p,
        std::filesystem::path{"/home/u/Library/Application "
                              "Support/OpenImageDebugger/imgui_settings.json"});
#endif
}

#if defined(__linux__)
TEST(ConfigPath, XdgOverridesHomeOnLinux) {
    const auto p = config_file_path_from_env("/xdg", "/home/u", nullptr);
    EXPECT_EQ(
        p, std::filesystem::path{"/xdg/OpenImageDebugger/imgui_settings.json"});
}
#endif
