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

#ifndef HOST_UI_SVG_ICON_CACHE_H_
#define HOST_UI_SVG_ICON_CACHE_H_

#include <array>

#include <GL/gl.h>

namespace oid::host {

// Identifies one of the embedded Qt-parity SVG icons
// (src/host/ui/icons, files named *_svg.h), used to key SvgIconCache's
// textures.
enum class IconId {
    CHANNEL_RED,
    CHANNEL_GREEN,
    CHANNEL_BLUE,
    CHANNEL_ALPHA,
    LOWER_UPPER_BOUND,
    GO_TO_X,
    GO_TO_Y,
};

// Lazily rasterizes and caches the embedded SVG icons as GL textures, the
// ImGui-frontend counterpart of the legacy Qt frontend's setVectorIcon (see
// tag legacy-qt) -- both rasterize a fixed-size vector icon once and hold
// onto a GPU resource, rather than doing so every frame. Unlike
// ThumbnailCache, these
// icons are static assets (not tied to a Stage/revision), so each IconId
// caches at most one texture for the life of this instance -- see
// texture_for()'s "first size wins" note.
class SvgIconCache {
  public:
    // `content_scale` is the window's HiDPI content scale (e.g. 2.0 on
    // Retina), used the same way ThumbnailCache uses it: the requested
    // logical display size is multiplied by it to pick the backing texture's
    // pixel size, so icons stay crisp on HiDPI displays (Qt parity:
    // setVectorIcon's devicePixelRatio-scaled pixmap).
    explicit SvgIconCache(float content_scale);
    ~SvgIconCache(); // deletes all cached GL textures

    SvgIconCache(const SvgIconCache&) = delete;
    SvgIconCache& operator=(const SvgIconCache&) = delete;
    SvgIconCache(SvgIconCache&&) = delete;
    SvgIconCache& operator=(SvgIconCache&&) = delete;

    // Returns the GL texture id holding `id`'s icon rasterized to display at
    // logical_w x logical_h (backing texels = logical size x content_scale_,
    // see ctor doc). Rasterizes and uploads on the first call for a given
    // `id`; later calls return the cached texture. 0 if rasterization ever
    // failed for `id` -- that failure is cached too, so a bad icon is not
    // re-rasterized every frame -- callers are expected to fall back to a
    // text label in that case. `logical_w`/`logical_h` are only honored on
    // the first call for a given `id`; a later call with a different size is
    // silently served the first size's texture (call sites request a fixed
    // size, so this never actually happens in practice). Requires a current
    // GL context.
    [[nodiscard]] GLuint texture_for(IconId id, int logical_w, int logical_h);

  private:
    struct Entry {
        GLuint tex{0};
        bool failed{false};
        bool cached{false};
    };

    // Derived from the enum's last value so adding an IconId can never
    // silently index entries_ out of bounds.
    static constexpr std::size_t ICON_COUNT =
        static_cast<std::size_t>(IconId::GO_TO_Y) + 1;

    float content_scale_;
    std::array<Entry, ICON_COUNT> entries_{};
};

} // namespace oid::host

#endif // HOST_UI_SVG_ICON_CACHE_H_
