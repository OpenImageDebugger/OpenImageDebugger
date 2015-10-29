#include <GL/glew.h>
#define GLFW_INCLUDE_GL3
#define GLFW_NO_GLU
#include <GLFW/glfw3.h>

#include "camera.hpp"
#include "stage.hpp"
#include "window.hpp"

void Camera::window_resized(int w, int h) {
    projection.setOrthoProjection(w/2.0, h/2.0, -1.0f, 1.0f);
    glViewport(0, 0, w, h);
}

void Camera::update() {
    float mouseX = stage->window->mouseX();
    float mouseY = stage->window->mouseY();
    if(stage->window->isMouseDown()) {
        // Mouse is down. Update camera_pos_x_/camera_pos_y_
        camera_pos_x_ += (mouseX-last_mouse_x)/zoom;
        camera_pos_y_ += -(mouseY-last_mouse_y)/zoom;
        model.setFromST(1.0/zoom, 1.0/zoom, 1.0, -camera_pos_x_, -camera_pos_y_, 0.f);
    }
    last_mouse_x = mouseX;
    last_mouse_y = mouseY;
}

bool Camera::post_initialize() {
    int w = stage->window->window_width();
    int h = stage->window->window_height();

    Buffer* buffer_component = stage->getComponent<Buffer>("buffer_component");
    float buffPosX = buffer_component->posX();
    float buffPosY = buffer_component->posY();

    camera_pos_x_ = -buffPosX, camera_pos_y_ = -buffPosY;
    model.setFromST(1.0/zoom, 1.0/zoom, 1.0, -camera_pos_x_, -camera_pos_y_, 0.f);

    window_resized(w, h);
    set_initial_zoom();

    return true;
}

void Camera::set_initial_zoom() {
    int win_w = stage->window->window_width();
    int win_h = stage->window->window_height();
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
    model.setFromST(1.0/zoom, 1.0/zoom, 1.0, -camera_pos_x_, -camera_pos_y_, 0.f);
}

bool Camera::post_buffer_update() {
    return post_initialize();
}
