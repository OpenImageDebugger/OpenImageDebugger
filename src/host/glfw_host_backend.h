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

#ifndef HOST_GLFW_HOST_BACKEND_H_
#define HOST_GLFW_HOST_BACKEND_H_

#include "host/host_backend.h"

#include <utility>
#include <vector>

struct GLFWwindow;

namespace oid::host {

class GlfwHostBackend final : public HostBackend {
  public:
    // Monitor work area (screen coordinates, e.g. glfwGetMonitorWorkarea)
    // used by window_visible_on() below.
    struct MonitorRect {
        int x;
        int y;
        int w;
        int h;
    };

    ~GlfwHostBackend() override;

    bool initialize(const char* title, int width, int height) override;
    void poll_events() override;
    [[nodiscard]] bool should_close() const override;
    [[nodiscard]] FramebufferSize framebuffer_size() const override;
    void begin_frame() override;
    void end_frame() override;
    void shutdown() override;

    // Toggle vsync on the current GL context (initialize() leaves it
    // current and defaults to on). Agent runs turn it off: FramePacer
    // paces the loop instead, so the swap must not block on a present the
    // OS may throttle for a background window.
    void set_vsync(bool enabled);

    // Refresh rate (Hz) of the primary monitor; 60 when GLFW cannot say
    // (headless / null video mode / non-positive rate). Queried once at
    // agent startup to pace the render loop.
    [[nodiscard]] int primary_refresh_rate_hz() const;

    [[nodiscard]] GLFWwindow* window() const {
        return window_;
    }

    // Current window size/position in screen coordinates (glfwGetWindowSize
    // / glfwGetWindowPos); {0, 0} when there is no window yet.
    [[nodiscard]] std::pair<int, int> window_size() const;
    [[nodiscard]] std::pair<int, int> window_position() const;

    // Moves the window (glfwSetWindowPos); no-op when there is no window.
    void set_window_position(int x, int y);

    // Every connected monitor's work area (glfwGetMonitors +
    // glfwGetMonitorWorkarea), for window_visible_on() below. Public: called
    // by main_imgui at startup to decide whether a saved window position is
    // still reachable, not just internally by this class.
    [[nodiscard]] std::vector<MonitorRect> monitors() const;

    // Pure, testable: does the window rect (x, y, w, h) overlap any monitor
    // work area? Used to discard a saved window position that would place
    // the window fully off-screen (e.g. a since-unplugged monitor).
    [[nodiscard]] static bool window_visible_on(
        int x, int y, int w, int h, const std::vector<MonitorRect>& monitors);

  private:
    GLFWwindow* window_{nullptr};
};

} // namespace oid::host

#endif // HOST_GLFW_HOST_BACKEND_H_
