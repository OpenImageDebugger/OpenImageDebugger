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

#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

using namespace oid::host;

namespace {

CliOptions parse(const std::vector<const char*>& args) {
    return parse_cli(static_cast<int>(args.size()), args.data());
}

} // namespace

TEST(CliOptionsTest, DefaultsWhenNoArgs) {
    const std::array<const char*, 1> argv = {"oidwindow"};
    const CliOptions options =
        parse_cli(static_cast<int>(argv.size()), argv.data());
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

TEST(CliOptionsTest, AcceptsLegacyHostAliases) {
    const CliOptions short_opt = parse({"oidwindow", "-h", "myhost"});
    EXPECT_EQ(short_opt.hostname, "myhost");

    const CliOptions long_opt = parse({"oidwindow", "--hostname", "myhost"});
    EXPECT_EQ(long_opt.hostname, "myhost");
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

TEST(CliOptionsTest, RejectsOutOfRangePort) {
    // A port that does not fit in the 1..65535 TCP range must not be narrowed
    // into a wrong port; keep the default instead.
    EXPECT_EQ(parse({"oidwindow", "--port", "70000"}).port, 9588);
    EXPECT_EQ(parse({"oidwindow", "--port", "65536"}).port, 9588);
    // The upper bound itself is still accepted.
    EXPECT_EQ(parse({"oidwindow", "--port", "65535"}).port, 65535);
}

TEST(CliOptionsTest, RejectsOverflowingOrTrailingGarbageNumbers) {
    // A digit string past INT_MAX is undefined behavior for std::atoi; it must
    // be rejected cleanly (from_chars reports out-of-range), keeping the
    // default rather than crashing or mis-parsing.
    EXPECT_EQ(parse({"oidwindow", "--port", "99999999999999999999"}).port,
              9588);
    // The whole token must parse -- trailing non-numeric bytes are rejected.
    EXPECT_EQ(parse({"oidwindow", "--port", "123abc"}).port, 9588);
    EXPECT_EQ(
        parse({"oidwindow", "--agent-debugger-pid", "99999999999999999999"})
            .agent_debugger_pid,
        std::nullopt);
}

TEST(CliOptionsTest, OpenFlagWithoutValueIsIgnored) {
    const CliOptions options = parse({"oidwindow", "-o"});
    EXPECT_TRUE(options.open_files.empty());
}

TEST(CliOptionsTest, ParsesAgentDebuggerPid) {
    const CliOptions options =
        parse({"oidwindow", "--agent-debugger-pid", "4200"});
    EXPECT_EQ(options.agent_debugger_pid, std::optional<int>{4200});
}

TEST(CliOptionsTest, AgentDebuggerPidDefaultsToNullopt) {
    const CliOptions options = parse({"oidwindow"});
    EXPECT_EQ(options.agent_debugger_pid, std::nullopt);
}

TEST(CliOptionsTest, NonNumericAgentDebuggerPidIsIgnored) {
    const CliOptions options =
        parse({"oidwindow", "--agent-debugger-pid", "notanumber"});
    EXPECT_EQ(options.agent_debugger_pid, std::nullopt);
}
