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

#include "host/cli_options.h"

#include <vector>

#include <gtest/gtest.h>

using namespace oid::host;

namespace {

CliOptions parse(const std::vector<const char*>& args) {
    return parse_cli(static_cast<int>(args.size()), args.data());
}

} // namespace

TEST(CliOptionsTest, DefaultsWhenNoArgs) {
    const char* argv[] = {"oidwindow"};
    const CliOptions options = parse_cli(1, argv);
    EXPECT_EQ(options.hostname, "127.0.0.1");
    EXPECT_EQ(options.port, 9588);
    EXPECT_TRUE(options.open_files.empty());
}

TEST(CliOptionsTest, ParsesPortAndHost) {
    const CliOptions options =
        parse({"oidwindow", "--host", "0.0.0.0", "--port", "1234"});
    EXPECT_EQ(options.hostname, "0.0.0.0");
    EXPECT_EQ(options.port, 1234);
}

TEST(CliOptionsTest, CollectsRepeatableOpenFlags) {
    const CliOptions options =
        parse({"oidwindow", "-o", "a.png", "--open", "b.npy", "-o", "c.jpg"});
    ASSERT_EQ(options.open_files.size(), 3u);
    EXPECT_EQ(options.open_files[0], "a.png");
    EXPECT_EQ(options.open_files[1], "b.npy");
    EXPECT_EQ(options.open_files[2], "c.jpg");
}

TEST(CliOptionsTest, IgnoresUnknownArgs) {
    const CliOptions options =
        parse({"oidwindow", "--frobnicate", "-o", "x.png"});
    ASSERT_EQ(options.open_files.size(), 1u);
    EXPECT_EQ(options.open_files[0], "x.png");
}

TEST(CliOptionsTest, InvalidPortLeavesDefault) {
    const CliOptions options = parse({"oidwindow", "--port", "notanumber"});
    EXPECT_EQ(options.port, 9588);
}

TEST(CliOptionsTest, OpenFlagWithoutValueIsIgnored) {
    const CliOptions options = parse({"oidwindow", "-o"});
    EXPECT_TRUE(options.open_files.empty());
}
