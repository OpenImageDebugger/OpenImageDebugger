#pragma once

#include "buffer.hpp"
#include "camera.hpp"
#include "buffer_values.hpp"

class Window;
class Stage {
public:
    Camera camera_component;
    Buffer buffer_component;
    BufferValues text_component;
    Window* window;

    bool initialize(Window* win, int w, int h, uint8_t* buffer, int buffer_width_i,
            int buffer_height_i, int channels, int type) {
        window = win;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

        camera_component.stage = this;
        buffer_component.stage = this;
        text_component.stage = this;

        buffer_component.initialize(buffer, buffer_width_i, buffer_height_i, channels, type);
        // The camera MUST be initialized AFTER the buffer
        camera_component.initialize(w, h);
        if(!text_component.initialize()) {
            return false;
        }

        return true;
    }

    void update() {
        camera_component.update();
        buffer_component.update();
        text_component.update();
    }

    void draw() {
        glClear(GL_COLOR_BUFFER_BIT);

        mat4 viewInv = camera_component.model.inv();
        buffer_component.draw(camera_component.projection, viewInv);
        text_component.draw(camera_component.projection, viewInv);
    }

    void scroll_callback(int delta) {
        camera_component.scroll_callback(delta);
    }

    void window_resized(int w, int h) {
        camera_component.window_resized(w, h);
    }
};
