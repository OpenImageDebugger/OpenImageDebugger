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

#include "platform/display_env.h"

#include <GLFW/glfw3.h>

namespace oid::platform {

// No-op on native: GLFW's own wheel/pointer path needs no takeover here --
// the wheel-capture and pointer-release-tracker workarounds this hook
// installs only exist to compensate for the non-native GLFW shim.
void install_input_workarounds(GLFWwindow* /*window*/) {
    /* no-op on native: no non-native GLFW shim here to work around; see comment
     * above */
}

// No-op on native: the GLFW window is resized directly by the OS/window
// manager, so there is no separate container size for the canvas to track
// (unlike the non-native embedding, where the canvas lives inside a host
// page element).
void sync_canvas_size(GLFWwindow* /*window*/) {
    /* no-op on native: the OS/window manager resizes the GLFW window directly;
     * see comment above */
}

std::optional<float> refresh_display_scale(float /*current*/) {
    return std::nullopt;
}

// No-op on native: GLFW delivers pointer button/motion events directly, so
// there is no dropped state to reconcile (unlike the non-native build,
// where stuck-button recovery compensates for events lost across the
// browser/JS shim boundary).
void reconcile_pointer_state() {
    /* no-op on native: GLFW delivers pointer events directly, nothing to
     * reconcile; see comment above */
}

const char* imgui_glsl_version() {
    return "#version 150";
}

float initial_content_scale(GLFWwindow* window) {
    float xscale = 1.0f;
    float yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    return xscale;
}

} // namespace oid::platform
