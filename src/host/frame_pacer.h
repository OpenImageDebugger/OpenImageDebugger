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

#ifndef HOST_FRAME_PACER_H_
#define HOST_FRAME_PACER_H_

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>

namespace oid::host {

// Paces agent-enabled render loop in place of vsync. With agent
// endpoint up, swap no longer blocks (vsync off), so the loop calls
// pace() each frame: sleeps on condition variable until the next
// frame deadline, so OS cannot stretch it the way it stretches a
// background window's present. An agent request wake()s the wait,
// drained immediately instead of waiting for the next presented frame.
class FramePacer {
  public:
    explicit FramePacer(std::chrono::nanoseconds period);

    // Thread-safe; called from any thread (an AgentServer serve thread,
    // right after it queues a request). Interrupts pace() in progress; if
    // none in progress, it is served on the next pace() entry.
    void wake();

    // Thread-safe; the new cadence takes effect immediately (the current
    // deadline is re-anchored to now + period). Used when the window moves
    // to a monitor with a different refresh rate.
    void set_period(std::chrono::nanoseconds period);

    // GL-thread only. Serves all pending burst wake() calls, one per
    // on_wake() invocation, until the frame deadline passes, advances the
    // deadline by one period -- clamped to now + period on overrun so a
    // slow frame never schedules a catch-up burst -- then returns.
    void pace(const std::function<void()>& on_wake);

  private:
    using Clock = std::chrono::steady_clock;

    std::chrono::nanoseconds period_;
    Clock::time_point deadline_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::uint64_t wakes_ = 0;    // total wake() calls
    std::uint64_t consumed_ = 0; // wakes pace() has already served
};

} // namespace oid::host

#endif // HOST_FRAME_PACER_H_
