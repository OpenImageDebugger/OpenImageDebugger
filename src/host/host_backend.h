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

#ifndef HOST_HOST_BACKEND_H_
#define HOST_HOST_BACKEND_H_

namespace oid::host {

struct FramebufferSize {
    int width{0};
    int height{0};
};

/// Abstract windowing/GL host. A backend owns the OS window, the GL context,
/// input polling, and the per-frame boundaries. No Qt, no ImGui here.
class HostBackend {
  public:
    virtual ~HostBackend() = default;

    /// Create the window + current GL context. Returns false on failure.
    virtual bool initialize(const char* title, int width, int height) = 0;

    /// Pump OS/input events for one frame.
    virtual void poll_events() = 0;

    /// True once the user/OS has requested the window close.
    [[nodiscard]] virtual bool should_close() const = 0;

    [[nodiscard]] virtual FramebufferSize framebuffer_size() const = 0;

    /// Make context current, set viewport to the framebuffer size, clear.
    virtual void begin_frame() = 0;

    /// Present the frame (swap buffers).
    virtual void end_frame() = 0;

    virtual void shutdown() = 0;
};

} // namespace oid::host

#endif // HOST_HOST_BACKEND_H_
