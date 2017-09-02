#pragma once

class GameObject;
class GLCanvas;
class mat4;
class Component {
public:
    GameObject* game_object = nullptr;
    GLCanvas* gl_canvas = nullptr;

    virtual bool initialize();

    virtual bool buffer_update();

    virtual bool post_buffer_update();

    virtual int render_index() const;

    virtual void mouse_drag_event(int mouse_x,
                                  int mouse_y);

    // Called after all components are initialized
    virtual bool post_initialize();

    virtual void update() = 0;

    virtual void draw(const mat4& projection,
                      const mat4& viewInv) = 0;

    virtual ~Component();
};
