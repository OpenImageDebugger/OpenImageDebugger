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

#include "host/ui/thumbnail_cache.h"

#include <unordered_set>

#include "host/glfw_canvas.h"
#include "host/util/transparent_string_hash.h"
#include "platform/gl_dialect.h"
#include "visualization/stage.h"

#include <ranges>

namespace oid::host {

ThumbnailCache::ThumbnailCache(GlfwCanvas& canvas, const float content_scale)
    : canvas_(canvas), render_w_(static_cast<int>(100.0f * content_scale)),
      render_h_(static_cast<int>(75.0f * content_scale)) {}

ThumbnailCache::~ThumbnailCache() {
    for (const auto& entry : entries_ | std::views::values) {
        if (entry.tex != 0) {
            canvas_.glDeleteTextures(1, &entry.tex);
        }
    }
}

void ThumbnailCache::begin_frame() {
    rendered_this_frame_ = false;
}

GLuint ThumbnailCache::texture_for(const std::string& name,
                                   const std::uint64_t revision,
                                   Stage* stage) {
    const auto it = entries_.find(name);

    if (const bool up_to_date =
            it != entries_.end() && it->second.revision == revision;
        up_to_date) {
        return it->second.failed ? 0 : it->second.tex;
    }

    // Entry missing (first time this name is seen) or stale (a re-plot
    // bumped the buffer's revision). Budgeted to at most one icon render
    // per frame across all callers (see class doc comment): if the budget
    // is already spent this frame, or the Stage isn't ready yet, fall back
    // to whatever texture is already cached -- a stale thumbnail reads
    // better than the row flashing blank on every re-plot -- rather than
    // forcing a render right now.
    if (rendered_this_frame_ || stage == nullptr) {
        return (it != entries_.end() && !it->second.failed) ? it->second.tex
                                                            : 0;
    }

    rendered_this_frame_ = true;

    // Reuse the cached texture object across re-renders (same fixed
    // render_w_ x render_h_ size every time, for the life of this instance);
    // only allocate a new one the first time this name is seen.
    GLuint tex = it != entries_.end() ? it->second.tex : 0;
    if (tex == 0) {
        canvas_.glGenTextures(1, &tex);
        canvas_.glBindTexture(GL_TEXTURE_2D, tex);
        canvas_.glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        canvas_.glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        canvas_.glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        canvas_.glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if (!canvas_.render_buffer_icon(*stage, render_w_, render_h_)) {
        // A previously-good icon exists for this name (same fixed
        // render_w_ x render_h_ texture object, just holding an older
        // frame's pixels): keep serving it rather than overwriting the
        // entry with failed=true, which would blank the row -- a stale
        // thumbnail reads better than the row flashing blank (class doc
        // comment above). Deliberately leave entries_[name] untouched
        // (still at its old revision, not `revision`) rather than
        // insert_or_assign-ing a "failed" record at the new revision: that
        // keeps up_to_date false on the next texture_for() call for this
        // name, so the render is retried next frame (budget permitting)
        // instead of being stuck until another revision bump ever comes.
        // No texture is allocated/leaked here: `tex` above is the reused
        // existing object, not a fresh glGenTextures() result.
        if (it != entries_.end() && !it->second.failed && it->second.tex != 0) {
            return it->second.tex;
        }
        // No prior good texture to fall back on (first render for this
        // name, or the last attempt already failed) -- nothing to show but
        // blank, so record the failure at the current revision.
        entries_.insert_or_assign(name, Entry{revision, tex, /*failed=*/true});
        return 0;
    }

    // Upload the freshly-rendered icon bytes (CPU-side, in
    // stage->get_buffer_icon(), filled by render_buffer_icon() above) into
    // the cached GL texture so ImGui::Image can display it directly.
    const auto& dialect = oid::the_dialect();
    canvas_.glBindTexture(GL_TEXTURE_2D, tex);
    canvas_.glTexImage2D(GL_TEXTURE_2D,
                         0,
                         static_cast<GLint>(dialect.icon_gl_internal_format),
                         render_w_,
                         render_h_,
                         0,
                         dialect.icon_gl_format,
                         GL_UNSIGNED_BYTE,
                         stage->get_buffer_icon().data());

    entries_.insert_or_assign(name, Entry{revision, tex, /*failed=*/false});
    return tex;
}

void ThumbnailCache::evict_missing(const std::vector<std::string>& live_names) {
    const std::
        unordered_set<std::string, TransparentStringHash, std::equal_to<>>
            live(live_names.begin(), live_names.end());
    std::erase_if(entries_, [&live, this](const auto& kv) {
        if (!live.contains(kv.first)) {
            if (kv.second.tex != 0) {
                canvas_.glDeleteTextures(1, &kv.second.tex);
            }
            return true;
        }
        return false;
    });
}

} // namespace oid::host
