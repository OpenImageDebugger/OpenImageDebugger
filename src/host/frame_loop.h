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

#ifndef HOST_FRAME_LOOP_H_
#define HOST_FRAME_LOOP_H_

#include <functional>

namespace oid::host {

class HostBackend;

/// Drives the per-frame cycle: poll input, begin frame, run the draw callback,
/// end frame. Backend-agnostic so it can be unit-tested against a fake.
class FrameLoop {
  public:
    FrameLoop(HostBackend& backend, std::function<void()> draw_frame);

    /// Run one iteration. Returns false if the backend requested close (in
    /// which case nothing was drawn), true after a rendered frame.
    bool tick();

    /// Loop until tick() returns false.
    void run();

    [[nodiscard]] int frame_count() const {
        return frame_count_;
    }

  private:
    HostBackend& backend_;
    std::function<void()> draw_frame_;
    int frame_count_{0};
};

} // namespace oid::host

#endif // HOST_FRAME_LOOP_H_
