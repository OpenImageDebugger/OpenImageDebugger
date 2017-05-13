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

    void add_component(const std::string& component_name,
                       std::shared_ptr<Component> component);

    mat4 get_pose();

    void mouse_drag_event(int mouse_x, int mouse_y);

    vec4 scale;
    vec4 position;
    float angle;

    const std::map<std::string, std::shared_ptr<Component>>& get_components();

private:
    std::map<std::string, std::shared_ptr<Component>> all_components;
};

#endif // GAME_OBJECT_H
