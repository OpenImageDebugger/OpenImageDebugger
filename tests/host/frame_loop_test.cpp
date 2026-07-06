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

#include "host/frame_loop.h"
#include "host/host_backend.h"

#include <gtest/gtest.h>

namespace {

using oid::host::FramebufferSize;
using oid::host::FrameLoop;
using oid::host::HostBackend;

// Backend that reports should_close() true after `close_after` polls, and
// records the order of calls so the loop's contract can be asserted.
class FakeHostBackend final : public HostBackend {
  public:
    explicit FakeHostBackend(int close_after) : close_after_(close_after) {}

    bool initialize(const char*, int, int) override {
        return true;
    }
    void poll_events() override {
        ++polls_;
    }
    [[nodiscard]] bool should_close() const override {
        return polls_ >= close_after_;
    }
    [[nodiscard]] FramebufferSize framebuffer_size() const override {
        return {640, 480};
    }
    void begin_frame() override {
        ++begins_;
    }
    void end_frame() override {
        ++ends_;
    }
    void shutdown() override {
        ++shutdowns_;
    }

    int polls_{0};
    int begins_{0};
    int ends_{0};
    int shutdowns_{0};
    int close_after_;
};

TEST(FrameLoop, TickRendersUntilBackendRequestsClose) {
    FakeHostBackend backend{/*close_after=*/3};
    int draws = 0;
    FrameLoop loop{backend, [&draws] { ++draws; }};

    loop.run();

    // close_after=3: ticks at polls 1 and 2 render; poll reaching 3 closes.
    EXPECT_EQ(loop.frame_count(), 2);
    EXPECT_EQ(draws, 2);
    EXPECT_EQ(backend.begins_, 2);
    EXPECT_EQ(backend.ends_, 2);
}

TEST(FrameLoop, TickReturnsFalseWhenClosing) {
    FakeHostBackend backend{/*close_after=*/1};
    // Draw callback is never invoked: close_after=1 means the very first
    // poll already hits the close threshold, so tick() returns before
    // drawing -- this test only asserts that early-return, not draw
    // behavior.
    FrameLoop loop{backend, [] { /* never invoked: close_after=1 returns before
                                    drawing; see comment above */
                   }};

    EXPECT_FALSE(loop.tick()); // first poll hits close threshold
    EXPECT_EQ(loop.frame_count(), 0);
}

TEST(FrameLoop, DrawRunsBetweenBeginAndEndFrame) {
    FakeHostBackend backend{/*close_after=*/2};
    int begins_at_draw = -1;
    int ends_at_draw = -1;
    FrameLoop loop{backend, [&backend, &begins_at_draw, &ends_at_draw] {
                       begins_at_draw = backend.begins_;
                       ends_at_draw = backend.ends_;
                   }};

    EXPECT_TRUE(loop.tick());

    EXPECT_EQ(begins_at_draw, 1); // begin_frame() already called
    EXPECT_EQ(ends_at_draw, 0);   // end_frame() not yet called
}

} // namespace
