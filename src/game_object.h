#ifndef GAME_OBJECT_H
#define GAME_OBJECT_H

#include <memory>

#include "math.hpp"
#include "camera.hpp"
#include "buffer.hpp"
#include "buffer_values.hpp"

class Stage;

class GameObject
{
    friend class Stage;
public:
    Stage* stage;

    GameObject();

    template<typename T>
    T* getComponent(std::string tag) {
        if(all_components.find(tag) == all_components.end())
            return nullptr;
        return dynamic_cast<T*>(all_components[tag].get());
    }

    bool initialize(GLCanvas *gl_canvas);

    bool post_initialize();

    void update();

    void draw(const mat4& projection, const mat4& viewInv);

    void add_component(const std::string& component_name,
                       std::shared_ptr<Component> component);

    mat4 get_pose();

    void mouse_drag_event(int mouse_x, int mouse_y);

    void set_render_index(int renderIndex);

    int get_render_index() const;

    vec4 scale;
    vec4 position;
    float angle;

private:
    std::map<std::string, std::shared_ptr<Component>> all_components;
    int renderIndex_;
};

#endif // GAME_OBJECT_H
