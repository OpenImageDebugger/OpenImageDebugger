#include <GL/glew.h>

#include "camera.hpp"
#include "stage.hpp"
#include "mainwindow.h"

void Camera::window_resized(int w, int h) {
    projection.setOrthoProjection(w/2.0, h/2.0, -1.0f, 1.0f);
    glViewport(0, 0, w, h);
}

void Camera::update() {
    float mouseX = gl_canvas->mouseX();
    float mouseY = gl_canvas->mouseY();
    if(gl_canvas->isMouseDown()) {
        // Mouse is down. Update camera_pos_x_/camera_pos_y_
        camera_pos_x_ += (mouseX-last_mouse_x)/zoom;
        camera_pos_y_ += (mouseY-last_mouse_y)/zoom;
        set_model_matrix();
    }
    last_mouse_x = mouseX;
    last_mouse_y = mouseY;
}

void Camera::reset_buffer_origin() {
    Buffer* buffer_component = stage->getComponent<Buffer>("buffer_component");

    buffer_origin_x_ = -buffer_component->buffer_width_f/2.0
                       - buffer_component->posX()/2.0;
    buffer_origin_y_ = -buffer_component->buffer_height_f/2.0
                       - buffer_component->posY()/2.0;

    set_model_matrix();
}

bool Camera::post_initialize() {
    reset_buffer_origin();

    int w = gl_canvas->width();
    int h = gl_canvas->height();

    window_resized(w, h);
    set_initial_zoom();

    return true;
}

void Camera::set_model_matrix() {
    model.setFromST(1.0/zoom, 1.0/zoom, 1.0,
            -camera_pos_x_-buffer_origin_x_,
            -camera_pos_y_-buffer_origin_y_, 0.f);
}

void Camera::set_initial_zoom() {
    int win_w = gl_canvas->width();
    int win_h = gl_canvas->height();
    Buffer* buff = stage->getComponent<Buffer>("buffer_component");
    float buf_w = buff->buffer_width_f;
    float buf_h = buff->buffer_height_f;

    if(win_w>buf_w && win_h>buf_h) {
        // Zoom in
        zoom_power_ += 1.0;
        float new_zoom = pow(zoom_factor, zoom_power_);
        while(win_w>new_zoom*buf_w && win_h>new_zoom*buf_h) {
            zoom_power_ += 1.0;
            new_zoom = pow(zoom_factor, zoom_power_);
        }
        zoom_power_ -= 1.0;
    } else if(win_w<buf_w || win_h<buf_h) {
        // Zoom out
        zoom_power_ -= 1.0;
        float new_zoom = pow(zoom_factor, zoom_power_);
        while(win_w<new_zoom*buf_w || win_h<new_zoom*buf_h) {
            zoom_power_ -= 1.0;
            new_zoom = pow(zoom_factor, zoom_power_);
        }
    }

    zoom = pow(zoom_factor, zoom_power_);
    set_model_matrix();
}

bool Camera::post_buffer_update() {
    reset_buffer_origin();
    return true;
}
