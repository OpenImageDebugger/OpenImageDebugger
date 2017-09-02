#include <cmath>

#include <GL/glew.h>

#include "camera.h"

#include "ui/gl_canvas.h"
#include "visualization/game_object.h"
#include "visualization/stage.h"

Camera& Camera::operator=(const Camera& cam) {
    zoom_power_ = cam.zoom_power_;
    buffer_origin_x_ = cam.buffer_origin_x_;
    buffer_origin_y_ = cam.buffer_origin_y_;
    camera_pos_x_ = cam.camera_pos_x_;
    camera_pos_y_ = cam.camera_pos_y_;
    canvas_width_ = cam.canvas_width_;
    canvas_height_ = cam.canvas_height_;
    angle_ = cam.angle_;

    if(game_object != nullptr) {
        update_object_pose();

        float zoom = 1.0/std::pow(zoom_factor, zoom_power_);
        game_object->scale = {zoom, zoom, 1.0, 0.0};
    }

    return *this;
}

void Camera::window_resized(int w, int h) {
    projection.setOrthoProjection(w/2.0, h/2.0, -1.0f, 1.0f);
    canvas_width_ = w;
    canvas_height_ = h;
}

void Camera::scroll_callback(float delta) {
    zoom_power_ += delta;
    float zoom = 1.0/std::pow(zoom_factor, zoom_power_);
    game_object->scale = {zoom, zoom, 1.0, 0.0};
}

void Camera::update() {
}

void Camera::reset_buffer_origin() {
    buffer_origin_x_ = buffer_origin_y_ = 0.0f;

    update_object_pose();
}

void Camera::update_object_pose() {
    game_object->position = {-camera_pos_x_-buffer_origin_x_,
                             -camera_pos_y_-buffer_origin_y_,
                             0, 1.0f};
}

bool Camera::post_initialize() {
    reset_buffer_origin();

    window_resized(gl_canvas->width(), gl_canvas->height());
    set_initial_zoom();

    return true;
}

void Camera::set_initial_zoom() {
    using std::pow;

    GameObject* buffer_obj = game_object->stage->getGameObject("buffer");
    Buffer* buff = buffer_obj->getComponent<Buffer>("buffer_component");
    float buf_w = buff->buffer_width_f;
    float buf_h = buff->buffer_height_f;
    zoom_power_ = 0.0;

    if(canvas_width_>buf_w && canvas_height_>buf_h) {
        // Zoom in
        zoom_power_ += 1.0;
        float new_zoom = pow(zoom_factor, zoom_power_);
        while(canvas_width_>new_zoom*buf_w && canvas_height_>new_zoom*buf_h) {
            zoom_power_ += 1.0;
            new_zoom = pow(zoom_factor, zoom_power_);
        }
        zoom_power_ -= 1.0;
    } else if(canvas_width_<buf_w || canvas_height_<buf_h) {
        // Zoom out
        zoom_power_ -= 1.0;
        float new_zoom = pow(zoom_factor, zoom_power_);
        while(canvas_width_<new_zoom*buf_w || canvas_height_<new_zoom*buf_h) {
            zoom_power_ -= 1.0;
            new_zoom = pow(zoom_factor, zoom_power_);
        }
    }

    float zoom = 1.0/pow(zoom_factor, zoom_power_);
    game_object->scale = {zoom, zoom, 1.0, 0.0};
}

void Camera::recenter_camera()
{
    camera_pos_x_ = camera_pos_y_ = 0.0f;
    reset_buffer_origin();
    set_initial_zoom();
}

void Camera::mouse_drag_event(int mouseX, int mouseY)
{
    // Mouse is down. Update camera_pos_x_/camera_pos_y_
    float zoom = 1.0/std::pow(zoom_factor, zoom_power_);

    camera_pos_x_ += mouseX*zoom;
    camera_pos_y_ += mouseY*zoom;

    update_object_pose();
}

float Camera::get_zoom() {
    return 1.0f/game_object->scale.x();
}

bool Camera::post_buffer_update() {
    reset_buffer_origin();
    return true;
}
