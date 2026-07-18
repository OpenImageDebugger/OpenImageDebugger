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

namespace oid::host {

FramePacer::FramePacer(std::chrono::nanoseconds period)
    : period_{period}, deadline_{Clock::now() + period} {}

void FramePacer::wake() {
    {
        const std::scoped_lock lock(mutex_);
        ++wakes_;
    }
    cv_.notify_one();
}

void FramePacer::pace(const std::function<void()>& on_wake) {
    std::unique_lock lock(mutex_);
    for (;;) {
        while (consumed_ < wakes_ && Clock::now() < deadline_) {
            // Coalesce: one on_wake serves the whole pending burst (drain()
            // empties the queue), so N rapid wakes cost one dispatch. The
            // deadline bound keeps a hot client that re-wakes during
            // on_wake() from starving the frame: once the deadline passes,
            // surplus wakes stay pending and the next pace() serves them on
            // entry.
            consumed_ = wakes_;
            lock.unlock();
            if (on_wake) {
                on_wake();
            }
            lock.lock();
        }
        if (Clock::now() >= deadline_) {
            break;
        }
        // Spurious wakeups are harmless: loop re-checks wake
        // counter and deadline.
        cv_.wait_until(lock, deadline_);
    }
    deadline_ += period_;
    if (const auto now = Clock::now(); deadline_ < now) {
        deadline_ = now + period_;
    }
}

} // namespace oid::host
