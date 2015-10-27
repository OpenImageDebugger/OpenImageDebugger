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

    bool initialize(Window* win, uint8_t* buffer, int buffer_width_i,
            int buffer_height_i, int channels, int type) {
        window = win;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

        Component* all_components[] = {
            &camera_component, &buffer_component, &text_component
        };

        buffer_component.buffer = buffer;
        buffer_component.channels = channels;
        buffer_component.type = type;
        buffer_component.buffer_width_f = static_cast<float>(buffer_width_i);
        buffer_component.buffer_height_f = static_cast<float>(buffer_height_i);

        for(auto comp: all_components) {
            comp->stage = this;
            if(!comp->initialize()) {
                return false;
            }
        }

        for(auto comp: all_components) {
            if(!comp->post_initialize())
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
