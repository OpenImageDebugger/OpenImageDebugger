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

#ifndef CAMERA_H_
#define CAMERA_H_

#include "component.h"
#include "math/linear_algebra.h"

namespace oid
{

class Camera final : public Component
{
  public:
    Camera(GameObject& game_object, GLCanvas& gl_canvas);

    ~Camera() override = default;

    Camera(const Camera& cam);

    Camera(Camera&& cam) noexcept;

    Camera& operator=(const Camera& cam);

    Camera& operator=(Camera&& cam) noexcept;

    static constexpr float zoom_factor{1.1f};

    mat4 projection{};

    vec4 mouse_position{vec4::zero()};

    void update() override;

    void draw(const mat4&, const mat4&) override
    {
        // Do nothing
    }

    [[nodiscard]] bool post_buffer_update() override;

    [[nodiscard]] bool post_initialize() override;

    EventProcessCode key_press_event(int key_code) override;

    void window_resized(int w, int h);

    void scroll_callback(float delta);

    void recenter_camera();

    void mouse_drag_event(int mouse_x, int mouse_y) override;

    [[nodiscard]] float compute_zoom() const;

    void move_to(float x, float y);

    [[nodiscard]] vec4 get_position() const;

  private:
    void update_object_pose() const;

    [[nodiscard]] std::pair<float, float> get_buffer_initial_dimensions() const;

    void scale_at(const vec4& center_ndc, float delta);

    void set_initial_zoom();

    void handle_key_events();

    float zoom_power_{0.0f};
    float camera_pos_x_{0.0f};
    float camera_pos_y_{0.0f};

    int canvas_width_{0};
    int canvas_height_{0};

    mat4 scale_{};
};

} // namespace oid

#endif // CAMERA_H_
