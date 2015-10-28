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

    return true;
}

bool Camera::post_buffer_update() {
    return post_initialize();
}
