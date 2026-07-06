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

#include "host/imgui_layer.h"

#include <imgui.h>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "host/ui_fonts.h"
#include "platform/display_env.h"

namespace oid::host {

ImGuiLayer::~ImGuiLayer() {
    shutdown();
}

bool ImGuiLayer::initialize(GLFWwindow* window, const float content_scale) {
    if (initialized_) {
        return true;
    }
    if (window == nullptr) {
        return false;
    }

    window_ = window;
    content_scale_ = content_scale;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    setup_ui_fonts(content_scale);

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        ImGui::DestroyContext();
        return false;
    }

    oid::platform::install_input_workarounds(window);

    if (!ImGui_ImplOpenGL3_Init(oid::platform::imgui_glsl_version())) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    initialized_ = true;
    return true;
}

void ImGuiLayer::begin_frame() {
    // Size the canvas/GLFW window BEFORE ImGui_ImplGlfw_NewFrame: the
    // backend derives DisplaySize (logical points) and
    // DisplayFramebufferScale from fresh state. Font-atlas work (display
    // scale changes) also happens before ImGui_ImplOpenGL3_NewFrame so the
    // destroyed font texture is recreated in the same frame. Both are
    // platform no-ops on native.
    oid::platform::sync_canvas_size(window_);
    if (const auto scale = oid::platform::refresh_display_scale(content_scale_);
        scale.has_value()) {
        content_scale_ = *scale;
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        setup_ui_fonts(content_scale_);
        io.Fonts->Build();
        ImGui_ImplOpenGL3_DestroyFontsTexture();
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    // Stuck-button recovery must run between the backend NewFrame and
    // ImGui::NewFrame so an injected release resolves this frame.
    oid::platform::reconcile_pointer_state();
    ImGui::NewFrame();
}

void ImGuiLayer::render() const {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiLayer::shutdown() {
    if (!initialized_) {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}

} // namespace oid::host
