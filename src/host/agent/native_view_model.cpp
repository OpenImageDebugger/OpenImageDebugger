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

#include <array>
#include <cmath>
#include <numbers>

#include "host/agent/wire_buffer_type.h"
#include "host/ui/panels/panel_accessors.h"
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
static_assert(static_cast<float>(ViewModel::ZOOM_FACTOR) ==
                  oid::Camera::ZOOM_FACTOR,
              "agent zoom factor must match Camera::ZOOM_FACTOR");

namespace {

constexpr double PI = std::numbers::pi;

// Converts a Buffer::rotation() radians value into the [0, 360) degrees
// range ViewState::rotation_deg reports.
double normalize_degrees(double radians) {
    double degrees = radians * (180.0 / PI);
    degrees = std::fmod(degrees, 360.0);
    if (degrees < 0.0) {
        degrees += 360.0;
    }
    return degrees;
}

// Pixel layout that isolates a single channel for Buffer's "specific
// channel" display mode (set_display_channel_mode(1)): index 0/1/2 -> R/G/B.
const char* isolated_layout(int index) {
    static constexpr std::array<const char*, 3> LAYOUTS{"rrra", "ggga", "bbba"};
    return LAYOUTS[static_cast<std::size_t>(index)];
}

// Layout restored by set_channel(-1, _) ("all"): the buffer's own declared
// pixel_layout when it's a valid 4-char string, else Buffer's own default
// ("rgba").
std::string natural_layout(const BufferRecord& record) {
    // Mirror Buffer::set_pixel_layout()'s validation so the restore call can
    // never no-op and leave a stale isolated layout installed when a buffer
    // declares a 4-char-but-invalid pixel_layout (reachable via a custom
    // Python type bridge, an unvalidated external input).
    const bool valid =
        record.pixel_layout.size() == 4 &&
        record.pixel_layout.find_first_not_of("rgba") == std::string::npos;
    return valid ? record.pixel_layout : "rgba";
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

std::optional<BufferInfo> NativeViewModel::buffer_at(std::size_t i) {
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

std::optional<BufferInfo> NativeViewModel::buffer_named(std::string_view name) {
    const auto idx = ui_.model_index_of(name);
    if (!idx.has_value()) {
        return std::nullopt;
    }
    return buffer_at(*idx);
}

bool NativeViewModel::read_pixels(std::string_view name,
                                  std::vector<std::byte>& out) {
    const auto idx = ui_.model_index_of(name);
    if (!idx.has_value()) {
        return false;
    }
    out = model_.at(*idx).bytes;
    return true;
}

oid::Stage* NativeViewModel::stage_for_name(std::string_view name) {
    const auto idx = ui_.model_index_of(name);
    if (!idx.has_value()) {
        return nullptr;
    }
    return stages_.stage_for(*idx);
}

bool NativeViewModel::select(std::string_view name) {
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

std::optional<ViewState> NativeViewModel::view_of(std::string_view name) {
    oid::Stage* stage = stage_for_name(name);
    if (!stage) {
        return std::nullopt;
    }
    const oid::Camera* camera = camera_of(*stage);
    const oid::Buffer* buffer = buffer_of(*stage);
    if (!camera || !buffer) {
        return std::nullopt;
    }

    ViewState state;
    state.buffer = std::string(name);
    const auto position = camera->get_position();
    state.center_x = static_cast<double>(position.x());
    state.center_y = static_cast<double>(position.y());
    state.zoom = static_cast<double>(camera->compute_zoom());
    state.rotation_deg =
        normalize_degrees(static_cast<double>(buffer->rotation()));
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

bool NativeViewModel::set_center(std::string_view name, double x, double y) {
    oid::Stage* stage = stage_for_name(name);
    if (!stage) {
        return false;
    }
    oid::Camera* camera = camera_of(*stage);
    if (!camera) {
        return false;
    }
    camera->move_to(static_cast<float>(x), static_cast<float>(y));
    return true;
}

bool NativeViewModel::set_zoom_power(std::string_view name, double power) {
    oid::Stage* stage = stage_for_name(name);
    if (!stage) {
        return false;
    }
    oid::Camera* camera = camera_of(*stage);
    if (!camera) {
        return false;
    }
    camera->set_zoom_power(static_cast<float>(power));
    return true;
}

bool NativeViewModel::set_rotation_rad(std::string_view name, double radians) {
    oid::Stage* stage = stage_for_name(name);
    if (!stage) {
        return false;
    }
    oid::Buffer* buffer = buffer_of(*stage);
    if (!buffer) {
        return false;
    }
    buffer->set_rotation(static_cast<float>(radians));
    return true;
}

bool NativeViewModel::set_channel(std::string_view name, int mode, int index) {
    const auto idx = ui_.model_index_of(name);
    if (!idx.has_value()) {
        return false;
    }
    oid::Stage* stage = stages_.stage_for(*idx);
    if (!stage) {
        return false;
    }
    oid::Buffer* buffer = buffer_of(*stage);
    if (!buffer) {
        return false;
    }

    if (mode == -1) {
        buffer->set_pixel_layout(natural_layout(model_.at(*idx)));
        buffer->set_display_channel_mode(-1);
        return true;
    }
    if (index < 0 || index > 2) {
        return false;
    }
    buffer->set_pixel_layout(isolated_layout(index));
    buffer->set_display_channel_mode(1);
    return true;
}

bool NativeViewModel::auto_contrast() {
    return ui_.contrast_enabled();
}

void NativeViewModel::set_auto_contrast(bool enabled) {
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
