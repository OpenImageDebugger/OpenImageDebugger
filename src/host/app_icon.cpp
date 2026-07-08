/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "host/app_icon.h"

#if !defined(__APPLE__) && !defined(__EMSCRIPTEN__)

#include <array>
#include <cstddef>
#include <iostream>
#include <utility>
#include <vector>

#include <GLFW/glfw3.h>

#include "host/app_icon_decode.h"
#include "host/ui/icons/app_icon_png.h"

// This is the single translation unit in this target that provides the
// stb_image_resize2 implementation (function bodies, not just declarations).
// The stb include dir is a SYSTEM include (-isystem, see
// src/CMakeLists.txt), which suppresses warnings from the vendored header
// under -Werror.
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

namespace oid::host {

void set_window_icon(GLFWwindow* const window) {
    if (window == nullptr) {
        return;
    }

    const DecodedImage base =
        decode_png_rgba(icons::APP_ICON_PNG, icons::APP_ICON_PNG_SIZE);
    if (base.rgba.empty()) {
        std::cerr << "[Warning] application icon could not be decoded; "
                     "window icon not set\n";
        return;
    }

    // Downscale to common small sizes so the window manager renders a crisp
    // title-bar/taskbar icon instead of scaling the 256px source itself.
    constexpr std::array<int, 4> sizes{256, 48, 32, 16};
    std::vector<std::vector<unsigned char>> pixel_buffers;
    pixel_buffers.reserve(sizes.size());
    std::vector<GLFWimage> images;
    images.reserve(sizes.size());

    for (const int size : sizes) {
        if (size == base.width && size == base.height) {
            pixel_buffers.push_back(base.rgba);
        } else {
            std::vector<unsigned char> scaled(static_cast<std::size_t>(size) *
                                              size * 4);
            const unsigned char* ok = stbir_resize_uint8_srgb(base.rgba.data(),
                                                              base.width,
                                                              base.height,
                                                              0,
                                                              scaled.data(),
                                                              size,
                                                              size,
                                                              0,
                                                              STBIR_RGBA);
            if (ok == nullptr) {
                continue;
            }
            pixel_buffers.push_back(std::move(scaled));
        }
        images.push_back(GLFWimage{size, size, pixel_buffers.back().data()});
    }

    if (!images.empty()) {
        glfwSetWindowIcon(
            window, static_cast<int>(images.size()), images.data());
    }
}

} // namespace oid::host

#elif defined(__APPLE__)

namespace oid::host {

// Defined in app_icon_mac.mm (Objective-C++). macOS has no per-window
// title-bar icon, so the application icon is set on the Dock tile instead.
void set_macos_dock_icon();

void set_window_icon(GLFWwindow* /*window*/) {
    set_macos_dock_icon();
}

} // namespace oid::host

#else // __EMSCRIPTEN__

struct GLFWwindow;

namespace oid::host {

// There is no window icon concept on the web, so this is a no-op.
void set_window_icon(GLFWwindow* /*window*/) {}

} // namespace oid::host

#endif // platform branch
