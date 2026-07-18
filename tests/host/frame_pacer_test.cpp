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

#include "host/frame_pacer.h"

#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

namespace {

using oid::host::FramePacer;

// All assertions are on ordering/counts, never durations: periods are
// milliseconds-scale so the suite stays fast, and no test asserts how long
// anything took (CI machines stall arbitrarily).

TEST(FramePacer, ReturnsAtDeadlineWithoutWakes) {
    FramePacer pacer{std::chrono::milliseconds(5)};
    int calls = 0;
    pacer.pace([&calls] { ++calls; });
    EXPECT_EQ(calls, 0);
}

TEST(FramePacer, PendingWakeIsServedOnEntry) {
    // 100 ms period: the pending wake must be served before the deadline
    // check, so the period is the tolerance against a CI scheduling stall
    // between wake() and pace() -- 5 ms was too tight for loaded VMs.
    FramePacer pacer{std::chrono::milliseconds(100)};
    pacer.wake();
    int calls = 0;
    pacer.pace([&calls] { ++calls; });
    EXPECT_EQ(calls, 1);
}

TEST(FramePacer, BurstOfWakesCoalescesIntoOneDrain) {
    FramePacer pacer{std::chrono::milliseconds(100)};
    pacer.wake();
    pacer.wake();
    pacer.wake();
    int calls = 0;
    pacer.pace([&calls] { ++calls; });
    EXPECT_EQ(calls, 1);
}

TEST(FramePacer, WakeDuringPaceRunsCallback) {
    // No single fragile timing window: the waker retries until a wake is
    // served, and pace() is re-entered until one lands -- a wake falling
    // between two pace() calls is served on the next entry, so both loops
    // terminate under any scheduling.
    FramePacer pacer{std::chrono::milliseconds(5)};
    std::atomic calls{0};
    std::atomic done{false};
    std::jthread waker([&pacer, &done] {
        while (!done.load()) {
            pacer.wake();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    while (calls.load() == 0) {
        pacer.pace([&calls] { ++calls; });
    }
    done.store(true);
    EXPECT_GE(calls.load(), 1);
}

TEST(FramePacer, WakeStormCannotStarveTheDeadline) {
    // A hot client can wake() again from inside on_wake (its reply lets it
    // send the next request immediately). pace() must still return once the
    // deadline passes -- with an unbounded drain loop this test never
    // returns instead of failing an assertion.
    FramePacer pacer{std::chrono::milliseconds(5)};
    int calls = 0;
    pacer.wake();
    pacer.pace([&pacer, &calls] {
        ++calls;
        pacer.wake(); // reentrant: keeps the backlog permanently non-empty
    });
    EXPECT_GE(calls, 1);
}

TEST(FramePacer, WakeAfterPaceReturnsIsSafeAndCarriesOver) {
    FramePacer pacer{std::chrono::milliseconds(100)};
    int calls = 0;
    pacer.pace([&calls] { ++calls; });
    pacer.wake(); // nobody waiting: must neither crash nor block
    pacer.pace([&calls] { ++calls; });
    EXPECT_EQ(calls, 1);
}

} // namespace
