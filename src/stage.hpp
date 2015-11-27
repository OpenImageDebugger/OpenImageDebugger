#ifndef STAGE_HPP
#define STAGE_HPP

#include <memory>
#include <map>

#include "stage.hpp"
#include "camera.hpp"
#include "buffer.hpp"
#include "buffer_values.hpp"

class Stage
{
public:

    Stage();

    template<typename T>
    T* getComponent(std::string tag) {
        if(all_components.find(tag) == all_components.end())
            return nullptr;
        return dynamic_cast<T*>(all_components[tag].get());
    }

    bool initialize(GLCanvas* gl_canvas, uint8_t* buffer, int buffer_width_i,
            int buffer_height_i, int channels, int type, int step, bool ac_enabled);

    bool buffer_update(uint8_t* buffer, int buffer_width_i,
            int buffer_height_i, int channels, int type, int step);

    void update();

    void draw();

    void scroll_callback(float delta);

    void resize_callback(int w, int h);

    void mouse_drag_event(int mouse_x, int mouse_y);

    bool contrast_enabled;

    std::vector<uint8_t> buffer_icon_;

private:
    std::map<std::string, std::shared_ptr<Component>> all_components;
};

#endif // STAGE_HPP
