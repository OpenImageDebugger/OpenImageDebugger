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

#include "host/agent/native_view_model.h"

#include <cmath>
#include <iostream>
#include <numbers>

#include "host/agent/natural_pixel_layout.h"
#include "host/agent/wire_buffer_type.h"
#include "host/ui/panels/panel_accessors.h"
#include "host/util/log_preview.h"
#include "visualization/components/buffer.h"
#include "visualization/components/camera.h"

namespace oid::host::agent {

// view_model.h is wasm-shared and must not include Camera, so
// ViewModel::ZOOM_FACTOR (double) hand-mirrors oid::Camera::ZOOM_FACTOR
// (float). This native-only translation unit already includes camera.h,
// so it is the place to catch the two constants drifting apart. Comparing
// double(1.1) to float(1.1f) promoted back to double is false -- rounding
// 1.1 to float precision changes the bit pattern -- so the double side is
// cast down to float before comparing at the engine's own precision.
static_assert(static_cast<float>(ViewModel::ZOOM_FACTOR) == Camera::ZOOM_FACTOR,
              "agent zoom factor must match Camera::ZOOM_FACTOR");

namespace {

constexpr double PI = std::numbers::pi;

// Converts a Buffer::rotation() radians value into the [0, 360) degrees
// range ViewState::rotation_deg reports.
double normalize_degrees(const double radians) {
    double degrees = radians * (180.0 / PI);
    degrees = std::fmod(degrees, 360.0);
    if (degrees < 0.0) {
        degrees += 360.0;
    }
    return degrees;
}

// Pixel layout that isolates a single channel for Buffer's "specific
// channel" display mode (set_display_channel_mode(1)): index 0/1/2 -> R/G/B.
// Reads ISOLATION_LAYOUTS (natural_pixel_layout.h) rather than its own
// literal array, so this and is_isolation_layout() can never drift apart on
// what counts as an isolation swizzle.
const char* isolated_layout(const int index) {
    return ISOLATION_LAYOUTS[static_cast<std::size_t>(index)];
}

} // namespace

NativeViewModel::NativeViewModel(IpcBufferModel& model,
                                 StageManager& stages,
                                 UiState& ui,
                                 std::shared_ptr<RenderCanvas> canvas)
    : model_(model), stages_(stages), ui_(ui), canvas_(std::move(canvas)) {}

std::size_t NativeViewModel::buffer_count() {
    return model_.size();
}

std::optional<BufferInfo> NativeViewModel::buffer_at(const std::size_t i) {
    if (i >= model_.size()) {
        return std::nullopt;
    }
    const BufferRecord& r = model_.at(i);
    // Report the type of the bytes actually served on the wire: FLOAT64
    // payloads are narrowed to float32 on ingest (see wire_buffer_type).
    return BufferInfo{r.variable_name,
                      r.display_name,
                      r.width,
                      r.height,
                      r.channels,
                      r.step,
                      static_cast<int>(wire_buffer_type(r.type)),
                      r.pixel_layout,
                      r.transpose};
}

std::optional<BufferInfo>
NativeViewModel::buffer_named(const std::string_view name) {
    const auto idx = ui_.model_index_of(name);
    if (!idx.has_value()) {
        return std::nullopt;
    }
    return buffer_at(*idx);
}

bool NativeViewModel::read_pixels(const std::string_view name,
                                  std::vector<std::byte>& out) {
    const auto idx = ui_.model_index_of(name);
    if (!idx.has_value()) {
        return false;
    }
    out = model_.at(*idx).bytes;
    return true;
}

Stage* NativeViewModel::stage_for_name(const std::string_view name) const {
    const auto idx = ui_.model_index_of(name);
    if (!idx.has_value()) {
        return nullptr;
    }
    return stages_.stage_for(*idx);
}

bool NativeViewModel::select(const std::string_view name) {
    const auto idx = ui_.model_index_of(name);
    if (!idx.has_value()) {
        return false;
    }
    ui_.select(*idx);
    return true;
}

std::optional<std::string> NativeViewModel::selected_name() {
    if (!ui_.has_selection()) {
        return std::nullopt;
    }
    return model_.variable_name_of(ui_.selected());
}

std::optional<ViewState> NativeViewModel::view_of(const std::string_view name) {
    Stage* stage = stage_for_name(name);
    if (!stage) {
        return std::nullopt;
    }
    const Camera* camera = camera_of(*stage);
    const Buffer* buffer = buffer_of(*stage);
    if (!camera || !buffer) {
        return std::nullopt;
    }

    ViewState state;
    state.buffer = std::string(name);
    const auto position = camera->get_position();
    state.center_x = static_cast<double>(position.x());
    state.center_y = static_cast<double>(position.y());
    state.zoom = static_cast<double>(camera->compute_zoom());
    state.rotation_deg = normalize_degrees(buffer->rotation());
    const int mode = buffer->get_display_channel_mode();
    state.channel = mode == -1
                        ? "all"
                        : std::to_string(buffer->get_selected_channel_index());
    state.auto_contrast = ui_.contrast_enabled();
    const auto [viewport_w, viewport_h] = viewport_size();
    state.viewport_w = viewport_w;
    state.viewport_h = viewport_h;
    return state;
}

bool NativeViewModel::set_center(const std::string_view name,
                                 const double x,
                                 const double y) {
    Stage* stage = stage_for_name(name);
    if (!stage) {
        return false;
    }
    Camera* camera = camera_of(*stage);
    if (!camera) {
        return false;
    }
    camera->move_to(static_cast<float>(x), static_cast<float>(y));
    return true;
}

bool NativeViewModel::set_zoom_power(const std::string_view name,
                                     const double power) {
    Stage* stage = stage_for_name(name);
    if (!stage) {
        return false;
    }
    Camera* camera = camera_of(*stage);
    if (!camera) {
        return false;
    }
    camera->set_zoom_power(static_cast<float>(power));
    return true;
}

bool NativeViewModel::set_rotation_rad(const std::string_view name,
                                       const double radians) {
    Stage* stage = stage_for_name(name);
    if (!stage) {
        return false;
    }
    Buffer* buffer = buffer_of(*stage);
    if (!buffer) {
        return false;
    }
    buffer->set_rotation(static_cast<float>(radians));
    return true;
}

bool NativeViewModel::set_channel(const std::string_view name,
                                  const int mode,
                                  const int index) {
    const auto idx = ui_.model_index_of(name);
    if (!idx.has_value()) {
        return false;
    }
    Stage* stage = stages_.stage_for(*idx);
    if (!stage) {
        return false;
    }
    Buffer* buffer = buffer_of(*stage);
    if (!buffer) {
        return false;
    }

    if (mode == -1) {
        // A guess is never stamped onto a buffer that already carries a
        // valid layout of its own: when the record cannot name a valid one
        // (see natural_pixel_layout.h), the buffer's current layout stays,
        // loudly, unless that current layout is itself an isolation swizzle
        // installed by the index arm below, in which case leaving it in
        // place would contradict the very "all channels" switch being
        // requested here, so the default is restored instead, loudly.
        const BufferRecord& record = model_.at(*idx);
        const std::string_view current_layout = buffer->get_pixel_layout();
        if (const auto decision = natural_pixel_layout(record, current_layout);
            decision.layout) {
            if (decision.cleared_isolation) {
                std::cerr << "[OID] set_channel(all) for '"
                          << log_preview(name, NAME_PREVIEW_CHARS)
                          << "': invalid pixel_layout '"
                          << log_preview(record.pixel_layout)
                          << "' on record; clearing the isolated '"
                          << log_preview(current_layout)
                          << "' layout, defaulting to '" << *decision.layout
                          << "'\n";
            }
            buffer->set_pixel_layout(*decision.layout);
        } else {
            std::cerr << "[OID] set_channel(all) for '"
                      << log_preview(name, NAME_PREVIEW_CHARS)
                      << "': invalid pixel_layout '"
                      << log_preview(record.pixel_layout)
                      << "' on record; keeping the buffer's current layout\n";
        }
        buffer->set_display_channel_mode(-1);
        return true;
    }
    if (index < 0 || index > 2) {
        return false;
    }
    // A buffer only carries the channels it has. Isolating one it does not
    // would leave view_of() reporting a channel that is not the one being
    // rendered, since a single-channel buffer always renders from red.
    if (index >= buffer->channels()) {
        return false;
    }
    buffer->set_pixel_layout(isolated_layout(index));
    buffer->set_display_channel_mode(1);
    return true;
}

bool NativeViewModel::auto_contrast() {
    return ui_.contrast_enabled();
}

void NativeViewModel::set_auto_contrast(const bool enabled) {
    // UiState only stores the flag; the per-frame toolbar/contrast-panel
    // update (see host/ui/panels/contrast_panel.cpp) is what actually pushes
    // it into each Stage's Buffer on the next frame.
    ui_.set_contrast_enabled(enabled);
}

std::pair<int, int> NativeViewModel::viewport_size() {
    if (!canvas_) {
        return {0, 0};
    }
    return {canvas_->render_width(), canvas_->render_height()};
}

} // namespace oid::host::agent
