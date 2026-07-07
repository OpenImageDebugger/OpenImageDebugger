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

#include "host/ui/panels/status_bar.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#include <imgui.h>

#include "host/glfw_canvas.h"
#include "host/ui/buffer_model.h"
#include "host/ui/panels/panel_accessors.h"
#include "host/ui/stage_manager.h"
#include "host/ui/ui_state.h"
#include "visualization/components/buffer.h"
#include "visualization/components/buffer_values.h"
#include "visualization/components/camera.h"
#include "visualization/game_object.h"
#include "visualization/stage.h"

namespace oid::host {

namespace {

// Ported 1:1 from MainWindow::get_stage_coordinates in the legacy Qt
// frontend (see tag legacy-qt): unprojects a mouse position given in
// pane-logical points (top-left origin) to buffer coordinates. Empty when
// the stage is missing its camera/buffer objects or components, or when
// the render surface is degenerate (guard not present in the Qt
// original: ImGui can report a zero-sized canvas while the window is
// minimized, which Qt's QOpenGLWidget never does).
std::optional<oid::vec4> stage_coordinates(oid::Stage& stage,
                                           const float pos_window_x,
                                           const float pos_window_y,
                                           const int render_w,
                                           const int render_h) {
    if (render_w <= 0 || render_h <= 0) {
        return std::nullopt;
    }

    const auto cam_obj = stage.get_game_object("camera");
    if (!cam_obj.has_value()) {
        return std::nullopt;
    }
    const auto cam_opt =
        cam_obj->get().get_component<oid::Camera>("camera_component");
    if (!cam_opt.has_value()) {
        return std::nullopt;
    }
    const auto& cam = cam_opt->get();

    const auto buffer_obj = stage.get_game_object("buffer");
    if (!buffer_obj.has_value()) {
        return std::nullopt;
    }
    const auto buffer_opt =
        buffer_obj->get().get_component<oid::Buffer>("buffer_component");
    if (!buffer_opt.has_value()) {
        return std::nullopt;
    }
    const auto& buffer = buffer_opt->get();

    const auto win_w = static_cast<float>(render_w);
    const auto win_h = static_cast<float>(render_h);
    const auto mouse_pos_ndc =
        oid::vec4{2.0f * (pos_window_x - win_w / 2) / win_w,
                  -2.0f * (pos_window_y - win_h / 2) / win_h,
                  0.0f,
                  1.0f};
    const auto view = cam_obj->get().get_pose().inv();
    const auto buff_pose = buffer_obj->get().get_pose();
    const auto vp_inv = (cam.projection() * view * buff_pose).inv();

    auto mouse_pos = vp_inv * mouse_pos_ndc;
    mouse_pos += oid::vec4(buffer.buffer_width_f() / 2.0f,
                           buffer.buffer_height_f() / 2.f,
                           0.0f,
                           0.0f);

    return mouse_pos;
}

// Negative sentinel: the Stage failed to initialize, or its camera
// GameObject/component isn't set up yet, so zoom isn't known this frame.
float compute_zoom_pct(oid::Stage* stage) {
    float zoom_pct = -1.0f;
    if (stage != nullptr) {
        if (const oid::Camera* cam = camera_of(*stage); cam != nullptr) {
            zoom_pct = cam->compute_zoom() * 100.0f;
        }
    }
    return zoom_pct;
}

// Formats the hovered pixel's value text (coordinates + per-channel
// values, plus float precision when applicable) — Qt parity with
// MainWindow::update_status_bar in the legacy Qt frontend (see tag
// legacy-qt).
std::string format_pixel_value_text(const oid::Buffer& buffer,
                                    oid::Stage& stage,
                                    const int px,
                                    const int py) {
    auto pixel = std::stringstream{};
    // Qt set fixed/setprecision(3) on its status stream (the legacy Qt
    // frontend; see tag legacy-qt) and that stream state governs how
    // get_pixel_info() prints float channels — replicate it or float
    // values render with default precision.
    pixel << std::fixed << std::setprecision(3);
    pixel << "(" << px << ", " << py << ") val=";
    buffer.get_pixel_info(pixel, px, py);
    if (oid::BufferType::Float64 == buffer.type() ||
        oid::BufferType::Float32 == buffer.type()) {
        if (const oid::BufferValues* values = values_of(stage);
            values != nullptr) {
            pixel << " precision=[" << values->get_float_precision() << "]";
        }
    }
    return pixel.str();
}

// Pixel value under the (last-hovered) cursor — Qt parity with
// MainWindow::update_status_bar in the legacy Qt frontend (see tag
// legacy-qt) minus its second zoom readout: this bar already shows one
// above.
// mouse_x()/mouse_y() persist the last hover position (main.cpp only
// feeds them while the canvas is hovered), so the readout persists after
// the cursor leaves the canvas, exactly like Qt's QLabel.
std::optional<std::string> format_hovered_pixel_text(oid::Stage* stage,
                                                     const GlfwCanvas& canvas) {
    if (stage == nullptr) {
        return std::nullopt;
    }
    const oid::Buffer* buffer = buffer_of(*stage);
    if (buffer == nullptr) {
        return std::nullopt;
    }

    const auto coords = stage_coordinates(*stage,
                                          static_cast<float>(canvas.mouse_x()),
                                          static_cast<float>(canvas.mouse_y()),
                                          canvas.render_width(),
                                          canvas.render_height());
    if (!coords.has_value()) {
        return std::nullopt;
    }

    const auto px = static_cast<int>(std::floor(coords->x()));
    const auto py = static_cast<int>(std::floor(coords->y()));
    return format_pixel_value_text(*buffer, *stage, px, py);
}

} // namespace

void draw_status_bar(const UiState& ui,
                     const BufferModel& model,
                     StageManager& stages,
                     const GlfwCanvas& canvas) {
    std::string line;
    if (!ui.has_selection()) {
        line = "no buffer";
    } else {
        const std::size_t sel = ui.selected();
        const BufferRecord& rec = model.at(sel);

        oid::Stage* stage = stages.selected_stage(sel);
        const float zoom_pct = compute_zoom_pct(stage);

        line = std::format("{}   [{}x{}]   {}",
                           rec.display_name,
                           rec.width,
                           rec.height,
                           type_label(rec.type, rec.channels));
        if (zoom_pct >= 0.0f) {
            line +=
                std::format("   zoom: {}%", static_cast<int>(zoom_pct + 0.5f));
        }

        if (const auto pixel_text = format_hovered_pixel_text(stage, canvas);
            pixel_text.has_value()) {
            line += "   " + *pixel_text;
        }
    }

    // Append the most recent export result (or any other transient
    // ui-level notice) to the same line, parity with the Qt app's
    // combined QStatusBar text.
    if (!ui.status_message().empty()) {
        line += " | " + ui.status_message();
    }

    ImGui::TextUnformatted(line.c_str());
}

} // namespace oid::host
