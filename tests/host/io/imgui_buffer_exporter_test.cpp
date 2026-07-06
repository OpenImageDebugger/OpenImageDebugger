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

#include "host/io/imgui_buffer_exporter.h"
#include "support/scratch_dir.h"

#include <algorithm>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <gtest/gtest.h>

TEST(ImguiExporter, PngRoundTrip) {
    oid::BufferExporter::RgbaImage img;
    img.width = 2;
    img.height = 2;
    // clang-format off
    img.pixels = {255, 0,   0,   255,
                  0,   255, 0,   255,
                  0,   0,   255, 255,
                  9,   8,   7,   255};
    // clang-format on
    const auto p = oid::test::scratch_dir() / "oid_p5_rt.png";
    std::filesystem::remove(p);
    ASSERT_TRUE(oid::host::export_rgba_png(img, p.string()));
    int w{};
    int h{};
    int n{};
    unsigned char* data = stbi_load(p.string().c_str(), &w, &h, &n, 4);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(w, 2);
    EXPECT_EQ(h, 2);
    EXPECT_TRUE(std::equal(img.pixels.begin(), img.pixels.end(), data));
    stbi_image_free(data);
}

TEST(ImguiExporter, PngFailsOnBadPath) {
    oid::BufferExporter::RgbaImage img;
    img.height = 1;
    img.width = img.height;
    img.pixels = {0, 0, 0, 255};
    EXPECT_FALSE(oid::host::export_rgba_png(img, "/nonexistent_dir_zzz/x.png"));
}
