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

#ifndef HOST_IMGUI_LAYER_H_
#define HOST_IMGUI_LAYER_H_

struct GLFWwindow;

namespace oid::host {

/// RAII wrapper around Dear ImGui's GLFW + OpenGL3 backends.
class ImGuiLayer {
  public:
    ~ImGuiLayer();

    // Owns process-global ImGui backend state; copying or moving a live
    // layer has no meaningful semantics.
    ImGuiLayer() = default;
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

    // content_scale is the GLFW window content scale
    // (glfwGetWindowContentScale), used to rasterize the UI font atlas at
    // physical resolution for crisp HiDPI text (see host/ui_fonts.h).
    bool initialize(GLFWwindow* window, float content_scale);
    void begin_frame();
    void render() const; // draw the accumulated ImGui draw data
    void shutdown();

  private:
    bool initialized_{false};
    // Per-frame platform sync (platform::sync_canvas_size); no-op on native.
    GLFWwindow* window_{nullptr};
    // Last display scale the font atlas was rasterized at (devicePixelRatio
    // on non-native builds, GLFW content scale on native).
    float content_scale_{1.0f};
};

} // namespace oid::host

#endif // HOST_IMGUI_LAYER_H_
