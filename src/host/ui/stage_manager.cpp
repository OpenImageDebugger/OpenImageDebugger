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

#include "host/ui/stage_manager.h"

#include <iostream>
#include <span>
#include <unordered_set>
#include <utility>

#include "host/ui/transparent_string_hash.h"

namespace oid::host {

namespace {

oid::BufferParams params_from(const BufferRecord& rec) {
    return oid::BufferParams{
        .buffer = std::span<const std::byte>{rec.bytes},
        .buffer_width_i = rec.width,
        .buffer_height_i = rec.height,
        .channels = rec.channels,
        .type = rec.type,
        .step = rec.step,
        .pixel_layout = rec.pixel_layout,
        .transpose_buffer = rec.transpose,
    };
}

} // namespace

StageManager::StageManager(std::shared_ptr<RenderCanvas> canvas,
                           const BufferModel& model)
    : canvas_(std::move(canvas)), model_(model) {}

oid::Stage* StageManager::selected_stage(std::size_t sel) {
    return stage_for(sel);
}

oid::Stage* StageManager::stage_for(std::size_t i) {
    sync();

    if (i >= model_.size()) {
        return nullptr;
    }

    const auto it = by_name_.find(model_.variable_name_of(i));
    return it == by_name_.end() ? nullptr : it->second.stage.get();
}

void StageManager::sync() {
    std::unordered_set<std::string, TransparentStringHash, std::equal_to<>>
        live_names;
    live_names.reserve(model_.size());

    for (std::size_t i = 0; i < model_.size(); ++i) {
        const std::string& name = model_.variable_name_of(i);
        const std::uint64_t rev = model_.revision_of(i);
        live_names.insert(name);

        const auto it = by_name_.find(name);
        if (it == by_name_.end()) {
            // New buffer. On initialize() failure, log and leave no entry
            // so a later frame can retry once the underlying cause (e.g.
            // GL context) is resolved.
            //
            // The empty callback is Stage's on_render_update hook (used by
            // the legacy Qt frontend to request a repaint -- see tag
            // legacy-qt); the ImGui host already redraws every frame
            // unconditionally, so there is nothing for it to trigger here.
            auto stage = std::make_unique<oid::Stage>(canvas_, [] {
                /* no-op: ImGui host redraws every frame; see comment above */
            });
            if (stage->initialize(params_from(model_.at(i)))) {
                by_name_.try_emplace(name, Entry{std::move(stage), rev});
            } else {
                std::cerr << "[Error] failed to initialize Stage for buffer '"
                          << name << "'\n";
            }
        } else if (it->second.revision != rev) {
            // Re-plot: rebuild the Stage's GL buffer from the record's
            // current bytes while preserving the Stage's camera/zoom, then
            // record the new revision so this isn't repeated next sync().
            if (!it->second.stage->buffer_update(params_from(model_.at(i)))) {
                std::cerr << "[Error] failed to update Stage for buffer '"
                          << name << "'\n";
            }
            it->second.revision = rev;
        }
    }

    // Drop Stages for buffers no longer present in the model. Erasing here
    // destroys the Stage (and the span into its now-gone BufferRecord)
    // before any dangling reference could be observed.
    std::erase_if(by_name_, [&live_names](const auto& kv) {
        return !live_names.contains(kv.first);
    });
}

} // namespace oid::host
