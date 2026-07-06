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

#include "host/glfw_canvas.h"

#include <gtest/gtest.h>

#include <type_traits>

using oid::host::GlfwCanvas;

// The canvas owns a heap GL entry-point table via unique_ptr, so it must be
// move-only: a copy would double-free. Enforce that at compile time.
static_assert(!std::is_copy_constructible_v<GlfwCanvas>,
              "GlfwCanvas must be non-copyable (owns a unique_ptr GL table)");
static_assert(
    !std::is_copy_assignable_v<GlfwCanvas>,
    "GlfwCanvas must be non-copy-assignable (owns a unique_ptr GL table)");

TEST(GlfwCanvasTest, ReportsFramebufferSizeFromProvider) {
    GlfwCanvas canvas{nullptr, [] { return std::pair<int, int>{800, 600}; }};
    EXPECT_EQ(canvas.render_width(), 800);
    EXPECT_EQ(canvas.render_height(), 600);
}

TEST(GlfwCanvasTest, HasNoTextRendererBeforePhase5) {
    GlfwCanvas canvas{nullptr, [] { return std::pair<int, int>{1, 1}; }};
    EXPECT_EQ(canvas.get_text_renderer(), nullptr);
}
