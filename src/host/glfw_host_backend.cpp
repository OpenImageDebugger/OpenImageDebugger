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

#include "host/glfw_host_backend.h"

#include <algorithm>
#include <iostream>

#include "host/app_icon.h"
#include "platform/window_hints.h"

// macOS's system OpenGL headers (pulled in transitively by glfw3.h) mark the
// desktop GL entry points used below as deprecated in favor of Metal. This
// project still targets desktop GL (see src/platform/gl_dialect.h), so we
// silence the deprecation warning rather than treat it as a -Werror failure.
// Same pattern used by Dear ImGui's own GLFW+GL examples/backends.
#define GL_SILENCE_DEPRECATION

#include <GLFW/glfw3.h>

namespace oid::host {

GlfwHostBackend::~GlfwHostBackend() {
    shutdown();
}

bool GlfwHostBackend::initialize(const char* title, int width, int height) {
    glfwSetErrorCallback([](int code, const char* description) {
        std::cerr << "[GLFW] error " << code << ": " << description << '\n';
    });
    if (glfwInit() == GLFW_FALSE) {
        return false;
    }
    // Request a 3.2 context so ImGui's GL3 backend works.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
#if defined(__APPLE__)
    // macOS (NSGL) does not offer a compatibility profile above legacy 2.1: the
    // only 3.2+ context it exposes is core + forward-compatible. Requesting a
    // compat profile there makes glfwCreateWindow fail outright.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    // On Linux/Windows a compatibility profile keeps the project's legacy scene
    // shaders valid on most drivers while still exposing 3.2 entry points.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
#endif

    oid::platform::apply_platform_window_hints();

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        return false;
    }
    set_window_icon(window_);
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // vsync
    return true;
}

void GlfwHostBackend::poll_events() {
    glfwPollEvents();
}

bool GlfwHostBackend::should_close() const {
    return window_ == nullptr || glfwWindowShouldClose(window_) != 0;
}

FramebufferSize GlfwHostBackend::framebuffer_size() const {
    FramebufferSize size{};
    if (window_ != nullptr) {
        glfwGetFramebufferSize(window_, &size.width, &size.height);
    }
    return size;
}

void GlfwHostBackend::begin_frame() {
    const auto size = framebuffer_size();
    glfwMakeContextCurrent(window_);
    glViewport(0, 0, size.width, size.height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GlfwHostBackend::end_frame() {
    glfwSwapBuffers(window_);
}

void GlfwHostBackend::shutdown() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
}

std::pair<int, int> GlfwHostBackend::window_size() const {
    int w = 0;
    int h = 0;
    if (window_ != nullptr) {
        glfwGetWindowSize(window_, &w, &h);
    }
    return {w, h};
}

namespace {

// Wayland deliberately hides a window's global position from the client, so
// its GLFW backend implements neither glfwGetWindowPos nor glfwSetWindowPos:
// calling them does nothing and raises GLFW_FEATURE_UNIMPLEMENTED (error
// 65548, "The platform does not provide the window position"). Skip those
// calls there so we don't spam the error callback and can't act on a bogus
// {0,0} readback. The Emscripten GLFW shim has no glfwGetPlatform and no
// movable OS window, so treat it as position-less too. Must be called only
// after glfwInit() (glfwGetPlatform requires it) -- guaranteed here since a
// non-null window_ implies a successful initialize().
bool platform_supports_window_position() {
#if defined(__EMSCRIPTEN__)
    return false;
#else
    return glfwGetPlatform() != GLFW_PLATFORM_WAYLAND;
#endif
}

} // namespace

std::pair<int, int> GlfwHostBackend::window_position() const {
    int x = 0;
    int y = 0;
    if (window_ != nullptr && platform_supports_window_position()) {
        glfwGetWindowPos(window_, &x, &y);
    }
    return {x, y};
}

void GlfwHostBackend::set_window_position(int x, int y) {
    if (window_ != nullptr && platform_supports_window_position()) {
        glfwSetWindowPos(window_, x, y);
    }
}

std::vector<GlfwHostBackend::MonitorRect> GlfwHostBackend::monitors() const {
    std::vector<MonitorRect> result;
    int count = 0;
    GLFWmonitor** mons = glfwGetMonitors(&count);
    result.reserve(static_cast<std::size_t>((std::max)(count, 0)));
    for (int i = 0; i < count; ++i) {
        MonitorRect rect{};
        glfwGetMonitorWorkarea(mons[i], &rect.x, &rect.y, &rect.w, &rect.h);
        result.push_back(rect);
    }
    return result;
}

bool GlfwHostBackend::window_visible_on(
    int x, int y, int w, int h, const std::vector<MonitorRect>& monitors) {
    return std::ranges::any_of(monitors, [x, y, w, h](const MonitorRect& m) {
        return x < m.x + m.w && x + w > m.x && y < m.y + m.h && y + h > m.y;
    });
}

} // namespace oid::host
