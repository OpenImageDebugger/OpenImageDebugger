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

#include "visualization/shader_pixel_layout.h"

#include <gtest/gtest.h>

#include <string>

using oid::shader_pixel_layout;

// A single-channel buffer is a GL_RED texture, sampled as (r, 0, 0, 1): a
// layout not starting with 'r' renders black, as Eigen's declared "bgra" did.
TEST(ShaderPixelLayoutTest, SingleChannelAlwaysSamplesRed) {
    for (const auto* declared : {"bgra", "grba", "abgr", "rgba"}) {
        EXPECT_EQ(shader_pixel_layout(std::string{declared}, 1), "rgba")
            << "declared layout: " << declared;
    }
}

// Channel order is real data for multi-channel buffers and must survive: BGRA
// image data would otherwise come out with red and blue swapped.
TEST(ShaderPixelLayoutTest, MultiChannelKeepsDeclaredOrder) {
    EXPECT_EQ(shader_pixel_layout(std::string{"bgra"}, 3), "bgra");
    EXPECT_EQ(shader_pixel_layout(std::string{"bgra"}, 4), "bgra");
    EXPECT_EQ(shader_pixel_layout(std::string{"rgba"}, 2), "rgba");
}

// Channel selection rotates the layout so the wanted component comes first;
// the texture still carries it, so the rotation must not flatten back to red.
TEST(ShaderPixelLayoutTest, ChannelSelectionOnMultiChannelIsPreserved) {
    EXPECT_EQ(shader_pixel_layout(std::string{"grba"}, 3), "grba");
    EXPECT_EQ(shader_pixel_layout(std::string{"bgra"}, 3), "bgra");
}

// Must agree with what shader_pixel_layout() samples, and it bounds the
// unclamped buffer[pos + channel] loop in BufferValues::draw_pixel_values().
TEST(SelectedChannelIndexTest, LayoutNamesTheChannelForMultiChannelBuffers) {
    EXPECT_EQ(oid::selected_channel_index(std::string{"rrra"}, 3), 0);
    EXPECT_EQ(oid::selected_channel_index(std::string{"ggga"}, 3), 1);
    EXPECT_EQ(oid::selected_channel_index(std::string{"bbba"}, 3), 2);
    EXPECT_EQ(oid::selected_channel_index(std::string{"bgra"}, 4), 2);
}

TEST(SelectedChannelIndexTest, ChannelTheBufferDoesNotHaveResolvesToZero) {
    // Single channel: shader_pixel_layout() forces "rgba", so red is what is
    // actually rendered and red is what must be reported.
    EXPECT_EQ(oid::selected_channel_index(std::string{"ggga"}, 1), 0);
    EXPECT_EQ(oid::selected_channel_index(std::string{"bbba"}, 1), 0);
    // Two channels: a GL_RG texture has no blue component to select.
    EXPECT_EQ(oid::selected_channel_index(std::string{"bbba"}, 2), 0);
    EXPECT_EQ(oid::selected_channel_index(std::string{"ggga"}, 2), 1);
}

TEST(SelectedChannelIndexTest, EmptyLayoutIsZero) {
    EXPECT_EQ(oid::selected_channel_index(std::string{}, 1), 0);
    EXPECT_EQ(oid::selected_channel_index(std::string{}, 3), 0);
}
