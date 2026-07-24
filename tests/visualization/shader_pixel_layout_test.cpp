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

// A single-channel buffer is a GL_RED texture, sampled as (r, 0, 0, 1). The
// fragment shader derives its source component from the layout's first
// character, so any layout not starting with 'r' would read a constant zero
// and render the buffer uniformly black regardless of contrast. This is the
// defect a declared "bgra" on an always-single-channel type (Eigen) produced.
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

// Selecting one channel of a multi-channel buffer for display rotates the
// layout so the wanted component comes first. The texture still carries that
// component, so the rotation must not be flattened back to red.
TEST(ShaderPixelLayoutTest, ChannelSelectionOnMultiChannelIsPreserved) {
    EXPECT_EQ(shader_pixel_layout(std::string{"grba"}, 3), "grba");
    EXPECT_EQ(shader_pixel_layout(std::string{"bgra"}, 3), "bgra");
}

// The selected index must agree with what shader_pixel_layout() causes to be
// sampled, or the viewer reports one channel while rendering another. It also
// bounds the channel loop in BufferValues::draw_pixel_values(), which indexes
// the pixel as buffer[pos + channel] without clamping to the channel count.
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
