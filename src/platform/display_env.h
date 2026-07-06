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

#ifndef PLATFORM_DISPLAY_ENV_H_
#define PLATFORM_DISPLAY_ENV_H_

#include <optional>

struct GLFWwindow;

namespace oid::platform {

// Platform seam: the display/input environment quirks of the platform, called
// unconditionally by the host layer. Native implementations are no-ops or
// thin GLFW pass-throughs; the non-native implementations carry the
// GLFW-shim workarounds.

// One-time, after ImGui_ImplGlfw_InitForOpenGL: take over the wheel path and
// install the pointer-release tracker (non-native); no-op native.
void install_input_workarounds(GLFWwindow* window);

// Per-frame, BEFORE the ImGui backend NewFrame calls: keep the canvas/GLFW
// logical size tracking its container (non-native); no-op native.
void sync_canvas_size(GLFWwindow* window);

// Per-frame, right after sync_canvas_size: when the device pixel ratio
// drifted from `current`, poke the platform's resize machinery and return
// the new ratio the font atlas must be re-rasterized at; nullopt otherwise.
// Native returns nullopt always (content-scale changes are not re-polled).
std::optional<float> refresh_display_scale(float current);

// Per-frame, after ImGui_ImplGlfw_NewFrame and before ImGui::NewFrame:
// reconcile pointer state the platform dropped (stuck-button recovery on
// non-native builds); no-op native.
void reconcile_pointer_state();

// GLSL version string for ImGui_ImplOpenGL3_Init.
const char* imgui_glsl_version();

// Content scale the UI/fonts start at.
float initial_content_scale(GLFWwindow* window);

} // namespace oid::platform

#endif // PLATFORM_DISPLAY_ENV_H_
