#pragma once
#include "math.hpp"

class GameObject;
class GLCanvas;
class Component {
public:
    GameObject* game_object = nullptr;
    GLCanvas* gl_canvas = nullptr;

    virtual bool initialize() {
        return true;
    }

    virtual bool buffer_update() {
        return true;
    }

    virtual bool post_buffer_update() {
        return true;
    }

    virtual int render_index() const {
        return 0;
    }

    virtual void mouse_drag_event(int /* mouse_x */,
                                  int /* mouse_y */) {}

    // Called after all components are initialized
    virtual bool post_initialize() {
        return true;
    }

    virtual void update() = 0;

    virtual void draw(const mat4& projection,
                      const mat4& viewInv) = 0;

    virtual ~Component(){}
};

