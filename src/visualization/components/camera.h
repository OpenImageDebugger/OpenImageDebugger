#ifndef CAMERA_H_
#define CAMERA_H_

#include "component.h"
#include "math/linear_algebra.h"


class Camera : public Component
{
  public:
    static constexpr float zoom_factor = 1.1;
    mat4 projection;

    vec4 mouse_position = vec4::zero();

    Camera& operator=(const Camera& cam);

    virtual void update();

    virtual void draw(const mat4&, const mat4&)
    {
    }

    virtual bool post_buffer_update();

    virtual bool post_initialize();

    void window_resized(int w, int h);

    void scroll_callback(float delta);

    void recenter_camera();

    void mouse_drag_event(int mouse_x, int mouse_y);

    float compute_zoom();

  private:
    void update_object_pose();

    void set_initial_zoom();

    float zoom_power_   = 0.0f;
    float camera_pos_x_ = 0.0f;
    float camera_pos_y_ = 0.0f;

    int canvas_width_;
    int canvas_height_;

    mat4 scale_;
};

#endif // CAMERA_H_
