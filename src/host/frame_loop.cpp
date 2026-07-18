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

#include <utility>

#include "host/host_backend.h"
#include "platform/main_loop.h"

namespace oid::host {

FrameLoop::FrameLoop(HostBackend& backend, std::function<void()> draw_frame)
    : backend_(backend), draw_frame_(std::move(draw_frame)) {}

bool FrameLoop::tick() {
    backend_.poll_events();
    if (backend_.should_close()) {
        return false;
    }
    backend_.begin_frame();
    if (draw_frame_) {
        draw_frame_();
    }
    backend_.end_frame();
    ++frame_count_;
    return true;
}

void FrameLoop::run() {
    platform::run_main_loop(*this);
}

} // namespace oid::host
