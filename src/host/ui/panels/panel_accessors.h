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

#ifndef HOST_UI_PANELS_PANEL_ACCESSORS_H_
#define HOST_UI_PANELS_PANEL_ACCESSORS_H_

#include "visualization/components/buffer.h"
#include "visualization/components/buffer_values.h"
#include "visualization/components/camera.h"
#include "visualization/game_object.h"
#include "visualization/stage.h"

namespace oid::host {

// Shared component accessors used by every panel that reaches into a Stage's
// GameObjects (toolbar, contrast, go-to, ...): each looks up a well-known
// GameObject ("buffer" or "camera") and then a well-known component on it,
// verified against the Qt app's Stage wiring, and returns nullptr when
// either isn't present yet (e.g. Stage::initialize() hasn't run), which
// callers must guard.

inline oid::Buffer* buffer_of(Stage& s) {
    const auto obj = s.get_game_object("buffer");
    if (!obj.has_value()) {
        return nullptr;
    }
    const auto comp = obj->get().get_component<Buffer>("buffer_component");
    return comp.has_value() ? &comp->get() : nullptr;
}

inline Camera* camera_of(Stage& s) {
    const auto obj = s.get_game_object("camera");
    if (!obj.has_value()) {
        return nullptr;
    }
    const auto comp = obj->get().get_component<Camera>("camera_component");
    return comp.has_value() ? &comp->get() : nullptr;
}

inline BufferValues* values_of(Stage& s) {
    const auto obj = s.get_game_object("buffer");
    if (!obj.has_value()) {
        return nullptr;
    }
    const auto comp = obj->get().get_component<BufferValues>("text_component");
    return comp.has_value() ? &comp->get() : nullptr;
}

} // namespace oid::host

#endif // HOST_UI_PANELS_PANEL_ACCESSORS_H_
