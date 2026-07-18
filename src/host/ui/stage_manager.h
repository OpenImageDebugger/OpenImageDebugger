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

#ifndef HOST_UI_STAGE_MANAGER_H_
#define HOST_UI_STAGE_MANAGER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "host/ui/buffer_model.h"
#include "host/util/transparent_string_hash.h"
#include "visualization/render_canvas.h"
#include "visualization/stage.h"

namespace oid::host {

// Owns one oid::Stage per buffer in the BufferModel, mirroring the Qt app's
// BufferData.stages map: each buffer gets its own Stage so per-buffer
// zoom/rotation/contrast persist across selection changes. Stages are
// created lazily (on first request) since initialize() requires a live GL
// context and uploads the buffer's pixels to a texture.
//
// Unlike MockBufferModel's fixed set, IpcBufferModel mutates at
// runtime: buffers are inserted, re-plotted (bytes replaced in place), and
// removed. Since each oid::Stage holds a std::span into a BufferRecord's
// bytes, indexing Stages 1:1 with the model would dangle a span the moment
// the model's slots shift or a record is replaced. StageManager instead
// keys Stages by BufferModel::variable_name_of() -- a buffer's stable
// identity -- and reconciles against the model's current contents (create
// new, buffer_update() on revision change, drop removed) every time a Stage
// is requested, via sync().
class StageManager {
  public:
    StageManager(std::shared_ptr<RenderCanvas> canvas,
                 const BufferModel& model);

    // Reconciles against the model, then returns the Stage for the selected
    // buffer index `sel`; nullptr if out of range or initialize() failed.
    [[nodiscard]] Stage* selected_stage(std::size_t sel);

    // Reconciles against the model, then returns the Stage for buffer index
    // `i`; nullptr if out of range or initialize() failed.
    [[nodiscard]] Stage* stage_for(std::size_t i);

  private:
    // A Stage plus the model-slot revision it was last built/updated from,
    // so sync() can tell an untouched buffer from a re-plotted one without
    // diffing bytes.
    struct Entry {
        std::unique_ptr<Stage> stage;
        std::uint64_t revision;
    };

    // Reconciles by_name_ against the model's current contents: creates a
    // Stage for a name not yet present, calls Stage::buffer_update() for a
    // name whose revision advanced (re-plot), and erases any name no longer
    // present in the model (removed buffer). Called at the top of
    // stage_for()/selected_stage() so callers always see a Stage set that
    // matches the model as of this frame.
    void sync();

    std::shared_ptr<RenderCanvas> canvas_;
    const BufferModel& model_;
    std::unordered_map<std::string,
                       Entry,
                       TransparentStringHash,
                       std::equal_to<>>
        by_name_;
};

} // namespace oid::host

#endif // HOST_UI_STAGE_MANAGER_H_
