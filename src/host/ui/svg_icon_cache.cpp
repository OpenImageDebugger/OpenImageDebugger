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

#include "host/ui/svg_icon_cache.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "host/ui/icons/label_alpha_channel_svg.h"
#include "host/ui/icons/label_blue_channel_svg.h"
#include "host/ui/icons/label_green_channel_svg.h"
#include "host/ui/icons/label_red_channel_svg.h"
#include "host/ui/icons/lower_upper_bound_svg.h"
#include "host/ui/icons/x_svg.h"
#include "host/ui/icons/y_svg.h"
#include "host/ui/svg_raster.h"

namespace oid::host {

namespace {

// Maps an IconId to its embedded SVG source (headers generated from
// the .svg files under src/resources/icons, one "<name>_svg.h" per
// icon). Returns
// {data, size}; every IconId has a matching array, so there is no
// "not found" case.
std::pair<const unsigned char*, std::size_t> svg_source_for(IconId id) {
    using enum IconId;
    switch (id) {
    case CHANNEL_RED:
        return {icons::kLabelRedChannelSvg, sizeof(icons::kLabelRedChannelSvg)};
    case CHANNEL_GREEN:
        return {icons::kLabelGreenChannelSvg,
                sizeof(icons::kLabelGreenChannelSvg)};
    case CHANNEL_BLUE:
        return {icons::kLabelBlueChannelSvg,
                sizeof(icons::kLabelBlueChannelSvg)};
    case CHANNEL_ALPHA:
        return {icons::kLabelAlphaChannelSvg,
                sizeof(icons::kLabelAlphaChannelSvg)};
    case LOWER_UPPER_BOUND:
        return {icons::kLowerUpperBoundSvg, sizeof(icons::kLowerUpperBoundSvg)};
    case GO_TO_X:
        return {icons::kXSvg, sizeof(icons::kXSvg)};
    case GO_TO_Y:
        return {icons::kYSvg, sizeof(icons::kYSvg)};
    }
    return {nullptr, 0};
}

} // namespace

SvgIconCache::SvgIconCache(float content_scale)
    : content_scale_(content_scale) {}

SvgIconCache::~SvgIconCache() {
    for (const Entry& entry : entries_) {
        if (entry.tex != 0) {
            glDeleteTextures(1, &entry.tex);
        }
    }
}

// The GL calls below are used raw (not via GlfwCanvas's dispatch table):
// glGenTextures/glBindTexture/glTexParameteri/glTexImage2D/glDeleteTextures
// are OpenGL 1.0/1.1 entry points, exported directly by every desktop GL
// library the app links (OpenGL.framework, libGL, opengl32) -- no
// glfwGetProcAddress loading is required for them, unlike the post-1.1
// functions GlfwCanvas has to load. Do not extrapolate this to newer GL
// functions.
GLuint SvgIconCache::texture_for(IconId id, int logical_w, int logical_h) {
    Entry& entry = entries_[static_cast<std::size_t>(id)];
    if (entry.cached) {
        return entry.failed ? 0 : entry.tex;
    }

    const auto out_w =
        static_cast<int>(static_cast<float>(logical_w) * content_scale_);
    const auto out_h =
        static_cast<int>(static_cast<float>(logical_h) * content_scale_);

    const auto [svg_data, svg_size] = svg_source_for(id);
    const std::vector<unsigned char> rgba =
        rasterize_svg(svg_data, svg_size, out_w, out_h);
    if (rgba.empty()) {
        entry = Entry{/*tex=*/0, /*failed=*/true, /*cached=*/true};
        return 0;
    }

    // Mirror ThumbnailCache's texture parameterization (thumbnail_cache.cpp)
    // -- linear filtering, clamp to edge -- so these icons scale/sample the
    // same way the buffer-list thumbnails do.
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA8,
                 out_w,
                 out_h,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 rgba.data());

    entry = Entry{tex, /*failed=*/false, /*cached=*/true};
    return tex;
}

} // namespace oid::host
