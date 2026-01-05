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

#include "camera.h"

#include <cmath>

#include "ui/gl_canvas.h"
#include "visualization/events.h"
#include "visualization/game_object.h"
#include "visualization/stage.h"

namespace oid
{

Camera::Camera(GameObject* game_object, GLCanvas* gl_canvas)
    : Component{game_object, gl_canvas}
{
}


Camera::Camera(const Camera& cam)
    : Component{cam}
    , zoom_power_{cam.zoom_power_}
    , camera_pos_x_{cam.camera_pos_x_}
    , camera_pos_y_{cam.camera_pos_y_}
    , canvas_width_{cam.canvas_width_}
    , canvas_height_{cam.canvas_height_}
    , scale_{cam.scale_}
{
    update_object_pose();
}


Camera& Camera::operator=(const Camera& cam)
{
    zoom_power_    = cam.zoom_power_;
    camera_pos_x_  = cam.camera_pos_x_;
    camera_pos_y_  = cam.camera_pos_y_;
    canvas_width_  = cam.canvas_width_;
    canvas_height_ = cam.canvas_height_;
    scale_         = cam.scale_;

    update_object_pose();

    return *this;
}


void Camera::window_resized(const int w, const int h)
{
    projection.set_ortho_projection(static_cast<float>(w) / 2.0f,
                                    static_cast<float>(h) / 2.0f,
                                    -1.0f,
                                    1.0f);
    canvas_width_  = w;
    canvas_height_ = h;
}


void Camera::scroll_callback(const float delta)
{
    const auto mouse_x = static_cast<float>(gl_canvas_->mouse_x());
    const auto mouse_y = static_cast<float>(gl_canvas_->mouse_y());
    const auto win_w   = static_cast<float>(gl_canvas_->width());
    const auto win_h   = static_cast<float>(gl_canvas_->height());

    const auto mouse_pos_ndc = vec4{2.0f * (mouse_x - win_w / 2.0f) / win_w,
                                    -2.0f * (mouse_y - win_h / 2.0f) / win_h,
                                    0.0f,
                                    1.0f};

    scale_at(mouse_pos_ndc, delta);
}


void Camera::update()
{
    handle_key_events();
}

std::pair<float, float> Camera::get_buffer_initial_dimensions() const
{
    const auto buffer_obj = game_object_->stage->get_game_object("buffer");
    const auto buff = buffer_obj->get_component<Buffer>("buffer_component");

    const auto buf_dim =
        buffer_obj->get_pose() *
        vec4(buff->buffer_width_f, buff->buffer_height_f, 0, 1);

    const auto x = std::abs(buf_dim.x());
    const auto y = std::abs(buf_dim.y());

    return std::make_pair(x, y);
}

void Camera::update_object_pose() const
{
    if (game_object_ != nullptr) {
        const vec4 position{-camera_pos_x_, -camera_pos_y_, 0.0f, 1.0f};

        // Since the view matrix of the camera is inverted before being applied
        // to the world coordinates, the order in which the operations below are
        // applied to world coordinates during rendering will also be reversed
        const auto pose = scale_ * mat4::translation(position);

        game_object_->set_pose(pose);
    }
}


bool Camera::post_initialize()
{
    window_resized(gl_canvas_->width(), gl_canvas_->height());
    set_initial_zoom();
    update_object_pose();

    return true;
}


// Handle keyboard events at the update loop
void Camera::handle_key_events()
{
    using Key = KeyboardState::Key;

    auto event_intercepted = EventProcessCode::IGNORED;

    if (KeyboardState::is_modifier_key_pressed(
            KeyboardState::ModifierKey::Control)) {
        auto delta_pos = vec4{0.0f, 0.0f, 0.0f, 0.0f};

        if (KeyboardState::is_key_pressed(Key::Up)) {
            delta_pos.y()     = -1.0f;
            event_intercepted = EventProcessCode::INTERCEPTED;
        } else if (KeyboardState::is_key_pressed(Key::Down)) {
            delta_pos.y()     = 1.0f;
            event_intercepted = EventProcessCode::INTERCEPTED;
        }

        if (KeyboardState::is_key_pressed(Key::Left)) {
            delta_pos.x()     = -1.0f;
            event_intercepted = EventProcessCode::INTERCEPTED;
        } else if (KeyboardState::is_key_pressed(Key::Right)) {
            delta_pos.x()     = 1.0f;
            event_intercepted = EventProcessCode::INTERCEPTED;
        }

        if (event_intercepted == EventProcessCode::INTERCEPTED) {
            // Recompute zoom matrix to discard its internal translation
            camera_pos_x_ -= delta_pos.x() + scale_(0, 3);
            camera_pos_y_ -= delta_pos.y() + scale_(1, 3);

            const auto zoom = 1.0f / compute_zoom();
            scale_          = mat4::scale(vec4(zoom, zoom, 1.0f, 1.0f));

            update_object_pose();

            game_object_->request_render_update();
        }
    }
}


EventProcessCode Camera::key_press_event(int)
{
    using Key = KeyboardState::Key;

    const auto screen_center = vec4{0.0f, 0.0f, 0.0f, 1.0f};
    auto event_intercepted   = EventProcessCode::IGNORED;

    if (KeyboardState::is_modifier_key_pressed(
            KeyboardState::ModifierKey::Control)) {
        if (KeyboardState::is_key_pressed(Key::Plus)) {
            scale_at(screen_center, 1.0f);

            event_intercepted = EventProcessCode::INTERCEPTED;
        } else if (KeyboardState::is_key_pressed(Key::Minus)) {
            scale_at(screen_center, -1.0f);

            event_intercepted = EventProcessCode::INTERCEPTED;
        } else if (KeyboardState::is_key_pressed(Key::Left) ||
                   KeyboardState::is_key_pressed(Key::Right) ||
                   KeyboardState::is_key_pressed(Key::Up) ||
                   KeyboardState::is_key_pressed(Key::Down)) {
            // Prevent the arrow keys to propagate
            event_intercepted = EventProcessCode::INTERCEPTED;
        }
    }

    return event_intercepted;
}


void Camera::scale_at(const vec4& center_ndc, const float delta)
{
    // Check if lowest zoom value has been reached.
    auto new_delta{delta};
    if (new_delta < 0) {

        // Ratio which defines farthest zoom limit. (0; 1].
        // 1 forbids to zoom farther than window size.
        // 0 allows unlimited zoom-out.
        static constexpr auto ratio_lowest{0.75f};

        // Get initial dimensions of buffer (without zoom).
        const auto [buffer_width, buffer_height] =
            get_buffer_initial_dimensions();

        // Find the lowest allowed zoom ratio.
        const auto zoom_lowest_x =
            ratio_lowest * static_cast<float>(canvas_width_) / buffer_width;
        const auto zoom_lowest_y =
            ratio_lowest * static_cast<float>(canvas_height_) / buffer_height;
        const auto zoom_lowest = (std::min)(zoom_lowest_x, zoom_lowest_y);

        // Find the lowest allowed zoom power.
        const auto zoom_power_lowest =
            std::log(zoom_lowest) / std::log(zoom_factor);

        // Find the lowest allowed delta.
        const auto delta_lowest = zoom_power_lowest - zoom_power_;
        if (delta_lowest >= 0) {
            return;
        }

        // Update delta if view matrix is near its lowest zoom.
        new_delta = (std::max)(new_delta, delta_lowest);
    }

    // Check if greatest zoom value has been reached.
    if (new_delta > 0) {

        // Closest zoom power. It may be any positive value.
        // 1 forbids to zoom closer than original matrix scale.
        // infinity allows unlimited zoom-in.
        static constexpr auto zoom_power_greatest{50.0f};

        // Find the greatest allowed delta.
        const auto delta_greatest{zoom_power_greatest - zoom_power_};
        if (delta_greatest <= 0) {
            return;
        }

        // Update delta if view matrix is near its greatest zoom.
        new_delta = (std::min)(new_delta, delta_greatest);
    }

    const auto vp_inv = game_object_->get_pose() * projection.inv();

    const auto delta_zoom = std::pow(zoom_factor, -new_delta);

    const auto center_pos = scale_.inv() * vp_inv * center_ndc;

    // Since the view matrix of the camera is inverted before being applied
    // to the world coordinates, the order in which the operations below are
    // applied to world coordinates during rendering will also be reversed

    // clang-format off
    scale_ = scale_ *
        mat4::translation(center_pos) *
        mat4::scale(vec4(delta_zoom, delta_zoom, 1.0f, 1.0f)) *
        mat4::translation(-center_pos);
    // clang-format on

    // Update camera position and force the scale matrix to contain scale
    // only
    camera_pos_x_ = camera_pos_x_ - scale_(0, 3) / scale_(0, 0);
    camera_pos_y_ = camera_pos_y_ - scale_(1, 3) / scale_(1, 1);

    scale_(0, 3) = 0.0f;
    scale_(1, 3) = 0.0f;

    // Calls to compute_zoom will require the zoom_power_ parameter to be on
    // par with the accumulated delta_zooms
    zoom_power_ += new_delta;

    update_object_pose();
}


void Camera::set_initial_zoom()
{
    static constexpr auto zoom_power_step{0.1f};

    // Get initial dimensions of buffer (without zoom).
    const auto [init_buffer_width, init_buffer_height] =
        get_buffer_initial_dimensions();

    zoom_power_ = 0.0f;

    const auto canvas_width_f = static_cast<float>(canvas_width_);

    if (const auto canvas_height_f = static_cast<float>(canvas_height_);
        canvas_width_f > init_buffer_width &&
        canvas_height_f > init_buffer_height) {
        // Zoom in
        zoom_power_ += zoom_power_step;
        float new_zoom = compute_zoom();

        // Iterate until buffer can fit inside the canvas
        while (canvas_width_f > new_zoom * init_buffer_width &&
               canvas_height_f > new_zoom * init_buffer_height) {
            zoom_power_ += zoom_power_step;
            new_zoom = compute_zoom();
        }

        zoom_power_ -= zoom_power_step;
    } else if (canvas_width_f < init_buffer_width ||
               canvas_height_f < init_buffer_height) {
        // Zoom out
        zoom_power_ -= zoom_power_step;
        float new_zoom = compute_zoom();

        // Iterate until buffer can fit inside the canvas
        while (canvas_width_f < new_zoom * init_buffer_width ||
               canvas_height_f < new_zoom * init_buffer_height) {
            zoom_power_ -= zoom_power_step;
            new_zoom = compute_zoom();
        }
    }

    const auto zoom{1.0f / compute_zoom()};
    scale_ = mat4::scale(vec4(zoom, zoom, 1.0f, 1.0f));
}


float Camera::compute_zoom() const
{
    return std::pow(zoom_factor, zoom_power_);
}


void Camera::move_to(const float x, const float y)
{
    const auto buffer_obj = game_object_->stage->get_game_object("buffer");

    const auto buff = buffer_obj->get_component<Buffer>("buffer_component");
    const auto buf_dim =
        vec4(buff->buffer_width_f, buff->buffer_height_f, 0.0f, 1.0f);
    const auto centered_coord = buf_dim * 0.5f - vec4(x, y, 0.0f, 0.0f);

    // Recompute zoom matrix to discard its internal translation
    const auto zoom{1.0f / compute_zoom()};
    scale_ = mat4::scale(vec4(zoom, zoom, 1.0f, 1.0f));

    const auto transformed_goal =
        scale_.inv() * buffer_obj->get_pose() * centered_coord;

    camera_pos_x_ = transformed_goal.x();
    camera_pos_y_ = transformed_goal.y();

    update_object_pose();
}


vec4 Camera::get_position() const
{
    const auto buffer_obj = game_object_->stage->get_game_object("buffer");

    const auto buff = buffer_obj->get_component<Buffer>("buffer_component");
    const auto buf_dim =
        vec4(buff->buffer_width_f, buff->buffer_height_f, 0.0f, 1.0f);
    const auto pos_vec = vec4{camera_pos_x_, camera_pos_y_, 0.0f, 1.0f};

    return buf_dim * 0.5f - buffer_obj->get_pose().inv() * scale_ * pos_vec;
}


void Camera::recenter_camera()
{
    camera_pos_x_ = camera_pos_y_ = 0.0f;

    set_initial_zoom();
    update_object_pose();
}


void Camera::mouse_drag_event(const int mouse_x, const int mouse_y)
{
    // Mouse is down. Update camera_pos_x_/camera_pos_y_
    camera_pos_x_ += static_cast<float>(mouse_x);
    camera_pos_y_ += static_cast<float>(mouse_y);

    update_object_pose();
}


bool Camera::post_buffer_update()
{
    return true;
}

} // namespace oid
