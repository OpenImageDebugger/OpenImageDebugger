#ifndef STAGE_HPP
#define STAGE_HPP

#include <memory>
#include <map>

#include "game_object.h"

class Stage
{
public:

    Stage();

    bool initialize(GLCanvas* gl_canvas, uint8_t* buffer, int buffer_width_i,
            int buffer_height_i, int channels, Buffer::BufferType type, int step, bool ac_enabled);

    bool buffer_update(uint8_t* buffer, int buffer_width_i,
            int buffer_height_i, int channels, Buffer::BufferType type, int step);

    GameObject* getGameObject(std::string tag);

    void update();

    void draw();

    void scroll_callback(float delta);

    void resize_callback(int w, int h);

    void mouse_drag_event(int mouse_x, int mouse_y);

    bool contrast_enabled;

    std::vector<uint8_t> buffer_icon_;

private:

    std::map<std::string, std::shared_ptr<GameObject>> all_game_objects;
};

#endif // STAGE_HPP
