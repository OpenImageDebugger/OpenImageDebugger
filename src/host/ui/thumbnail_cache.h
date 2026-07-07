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

#ifndef HOST_UI_THUMBNAIL_CACHE_H_
#define HOST_UI_THUMBNAIL_CACHE_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <GL/gl.h>

#include "host/ui/transparent_string_hash.h"

namespace oid {
class Stage;
} // namespace oid

namespace oid::host {

class GlfwCanvas;

// Caches small offscreen-rendered icon textures for the buffer-list panel,
// keyed by buffer name and refreshed on revision change --
// the ImGui-frontend counterpart of the legacy Qt frontend's per-row QIcon
// built from GLCanvas::render_buffer_icon (see tag legacy-qt).
//
// Rendering an icon binds the GL icon FBO, resizes the buffer's camera to
// the thumbnail dimensions, draws the Stage, and reads the pixels back --
// too expensive to do for every row on every frame. So texture_for() renders
// at most one icon per frame across ALL callers (tracked by
// rendered_this_frame_, reset in begin_frame()): callers are expected to
// call texture_for() for each row in list order every frame, and a missing
// or stale entry is (re)rendered opportunistically -- the first such row
// this frame gets rendered, the rest catch up on later frames -- rather than
// stalling the frame doing every row's readback at once.
class ThumbnailCache {
  public:
    // Display size of the icon slot in the buffer-list row (Qt parity: the
    // legacy Qt frontend's imageList iconSize, 67x50; see tag legacy-qt);
    // used by buffer_list_panel.cpp to size the ImGui::Image and lay out
    // the row.
    // This is deliberately smaller than the offscreen render size
    // (render_w_/render_h_ below) -- GL linear filtering downsamples the
    // higher-resolution render into this slot, the same way the legacy Qt
    // frontend scaled its 100x75 pixmap (its ICON_WIDTH_BASE/ICON_HEIGHT_BASE;
    // see tag legacy-qt) into the 67x50 icon.
    static constexpr int kDisplayW = 67;
    static constexpr int kDisplayH = 50;

    explicit ThumbnailCache(GlfwCanvas& canvas, float content_scale);
    ~ThumbnailCache(); // deletes all cached GL textures

    ThumbnailCache(const ThumbnailCache&) = delete;
    ThumbnailCache& operator=(const ThumbnailCache&) = delete;
    ThumbnailCache(ThumbnailCache&&) = delete;
    ThumbnailCache& operator=(ThumbnailCache&&) = delete;

    // Resets the per-frame render budget; call once per frame, before any
    // texture_for() call (see main.cpp's frame lambda).
    void begin_frame();

    // Returns the GL texture id holding `name`'s icon as of `revision`, or 0
    // if no up-to-date icon is available yet -- either it has never been
    // rendered, this frame's render budget is already spent, or rendering it
    // failed (e.g. the Stage has no camera yet). `stage` is the buffer's
    // Stage (may be nullptr, e.g. while StageManager is still initializing
    // it); texture_for() only renders when both the entry is stale/missing
    // and `stage` is non-null.
    [[nodiscard]] GLuint texture_for(const std::string& name,
                                     std::uint64_t revision,
                                     oid::Stage* stage);

    // Drops cached entries (and deletes their GL textures) for any name not
    // present in `live_names`, so a removed buffer's icon texture doesn't
    // leak. Call once per frame after the model has been reconciled (e.g.
    // right after ipc.poll()), before drawing the buffer list.
    void evict_missing(const std::vector<std::string>& live_names);

  private:
    struct Entry {
        std::uint64_t revision;
        GLuint tex;
        bool failed;
    };

    GlfwCanvas& canvas_;
    std::unordered_map<std::string,
                       Entry,
                       TransparentStringHash,
                       std::equal_to<>>
        entries_;
    bool rendered_this_frame_{false};
    // Offscreen render size (Qt parity: the legacy Qt frontend's
    // ICON_WIDTH_BASE/ICON_HEIGHT_BASE == 100x75, see tag legacy-qt, scaled
    // by the window's content scale so the
    // texture that later gets downsampled into the kDisplayW x kDisplayH
    // slot stays crisp on HiDPI displays). Set once in the ctor and fixed
    // for the life of this instance -- see texture_for()'s "same fixed
    // size every time" comment.
    int render_w_;
    int render_h_;
};

} // namespace oid::host

#endif // HOST_UI_THUMBNAIL_CACHE_H_
