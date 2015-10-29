#pragma once

#include "component.hpp"

class Camera : public Component {
public:
    static constexpr float zoom_factor = 1.2;
    float zoom = 1.0;
    mat4 projection;

    void update();
    void draw(const mat4& projection, const mat4& viewInv) {}

    bool post_buffer_update();
    bool post_initialize();

    void window_resized(int w, int h);
    
    void scroll_callback(int delta) {
        zoom_power_ += delta;
        zoom = pow(zoom_factor, zoom_power_);
        model.setFromST(1.0/zoom, 1.0/zoom, 1.0, -camera_pos_x_, -camera_pos_y_, 0.f);
    }

private:
    void set_initial_zoom();
    float zoom_power_ = 0.0;
    float camera_pos_x_ = 0.0;
    float camera_pos_y_ = 0.0;
    float last_mouse_x;
    float last_mouse_y;
};

