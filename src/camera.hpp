#pragma once

#include "component.hpp"

class Camera : public Component {
public:
    Camera& operator=(const Camera& cam);

    static constexpr float zoom_factor = 1.1;
    mat4 projection;

    void update();
    void draw(const mat4&, const mat4&) {}

    bool post_buffer_update();
    bool post_initialize();

    void window_resized(int w, int h);
    
    void scroll_callback(float delta);
    void set_initial_zoom();

    void recenter_camera();

    void mouse_drag_event(int mouse_x, int mouse_y);

    float get_zoom();

private:
    void reset_buffer_origin();
    void update_object_pose();

    float zoom_power_ = 0.0;
    float buffer_origin_x_ = 0.0;
    float buffer_origin_y_ = 0.0;
    float camera_pos_x_ = 0.0;
    float camera_pos_y_ = 0.0;
    int canvas_width_;
    int canvas_height_;
    float angle_ = 0.f;
};

