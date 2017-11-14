/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 GDB ImageWatch contributors
 * (github.com/csantosbh/gdb-imagewatch/)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <set>

#include "stage.h"

#include "game_object.h"
#include "visualization/components/background.h"
#include "visualization/components/buffer_values.h"
#include "visualization/components/camera.h"


using namespace std;


struct compareRenderOrder
{
    bool operator()(const Component* a, const Component* b) const
    {
        return a->render_index() < b->render_index();
    }
};


Stage::Stage()
{
}


bool Stage::initialize(GLCanvas* gl_canvas,
                       uint8_t* buffer,
                       int buffer_width_i,
                       int buffer_height_i,
                       int channels,
                       Buffer::BufferType type,
                       int step,
                       const string& pixel_layout,
                       bool transpose_buffer)
{
    std::shared_ptr<GameObject> camera_obj = std::make_shared<GameObject>();

    camera_obj->stage = this;
    camera_obj->add_component(
        "camera_component",
        std::make_shared<Camera>(camera_obj.get(), gl_canvas));
    camera_obj->add_component(
        "background_component",
        std::make_shared<Background>(camera_obj.get(), gl_canvas));

    all_game_objects["camera"] = camera_obj;

    std::shared_ptr<GameObject> buffer_obj = std::make_shared<GameObject>();

    buffer_obj->stage = this;
    buffer_obj->add_component(
        "text_component",
        std::make_shared<BufferValues>(buffer_obj.get(), gl_canvas));

    std::shared_ptr<Buffer> buffer_component =
        std::make_shared<Buffer>(buffer_obj.get(), gl_canvas);

    buffer_component->buffer          = buffer;
    buffer_component->channels        = channels;
    buffer_component->type            = type;
    buffer_component->buffer_width_f  = static_cast<float>(buffer_width_i);
    buffer_component->buffer_height_f = static_cast<float>(buffer_height_i);
    buffer_component->step            = step;
    buffer_component->transpose       = transpose_buffer;
    buffer_component->set_pixel_layout(pixel_layout);
    buffer_obj->add_component("buffer_component", buffer_component);

    all_game_objects["buffer"] = buffer_obj;

    for (const auto& go : all_game_objects) {
        if (!go.second->initialize()) {
            return false;
        }
    }

    for (const auto& go : all_game_objects) {
        if (!go.second->post_initialize()) {
            return false;
        }
    }

    return true;
}


bool Stage::buffer_update(uint8_t* buffer,
                          int buffer_width_i,
                          int buffer_height_i,
                          int channels,
                          Buffer::BufferType type,
                          int step,
                          const string& pixel_layout,
                          bool transpose_buffer)
{
    GameObject* buffer_obj = all_game_objects["buffer"].get();
    Buffer* buffer_component =
        buffer_obj->get_component<Buffer>("buffer_component");

    buffer_component->buffer          = buffer;
    buffer_component->channels        = channels;
    buffer_component->type            = type;
    buffer_component->buffer_width_f  = static_cast<float>(buffer_width_i);
    buffer_component->buffer_height_f = static_cast<float>(buffer_height_i);
    buffer_component->step            = step;
    buffer_component->transpose       = transpose_buffer;
    buffer_component->set_pixel_layout(pixel_layout);

    for (const auto& game_obj_it : all_game_objects) {
        GameObject* game_obj = game_obj_it.second.get();
        game_obj->stage      = this;
        for (const auto& comp : game_obj->get_components()) {
            if (!comp.second->buffer_update()) {
                return false;
            }
        }
    }

    for (const auto& game_obj_it : all_game_objects) {
        GameObject* game_obj = game_obj_it.second.get();
        for (const auto& comp : game_obj->get_components()) {
            if (!comp.second->post_buffer_update()) {
                return false;
            }
        }
    }

    return true;
}


GameObject* Stage::get_game_object(string tag)
{
    if (all_game_objects.find(tag) == all_game_objects.end()) {
        return nullptr;
    }

    return all_game_objects[tag].get();
}


void Stage::update()
{
    for (const auto& game_obj : all_game_objects) {
        game_obj.second->update();
    }
}


void Stage::draw()
{
    set<Component*, compareRenderOrder> ordered_components;

    // TODO use camera tags so I can have multiple cameras (useful for drawing
    // GUI)

    GameObject* camera_obj = all_game_objects["camera"].get();
    Camera* camera_component =
        camera_obj->get_component<Camera>("camera_component");

    if (camera_component == nullptr)
        return;

    mat4 view_inv = camera_obj->get_pose().inv();

    for (const auto& game_obj : all_game_objects) {
        for (const auto& component : game_obj.second->get_components()) {
            ordered_components.insert(component.second.get());
        }
    }

    for (const auto& component : ordered_components) {
        component->draw(camera_component->projection, view_inv);
    }
}


void Stage::scroll_callback(float delta)
{
    GameObject* cam_obj = all_game_objects["camera"].get();
    Camera* camera_component =
        cam_obj->get_component<Camera>("camera_component");
    camera_component->scroll_callback(delta);
}


void Stage::resize_callback(int w, int h)
{
    GameObject* cam_obj = all_game_objects["camera"].get();
    Camera* camera_component =
        cam_obj->get_component<Camera>("camera_component");
    camera_component->window_resized(w, h);
}


void Stage::mouse_drag_event(int mouse_x, int mouse_y)
{
    for (const auto& game_obj : all_game_objects) {
        game_obj.second->mouse_drag_event(mouse_x, mouse_y);
    }
}


void Stage::mouse_move_event(int mouse_x, int mouse_y)
{
    for (const auto& game_obj : all_game_objects) {
        game_obj.second->mouse_move_event(mouse_x, mouse_y);
    }
}


void Stage::key_press_event(int key_code)
{
    for (const auto& game_obj : all_game_objects) {
        game_obj.second->key_press_event(key_code);
    }
}


void Stage::go_to_pixel(int x, int y)
{
    GameObject* cam_obj = all_game_objects["camera"].get();
    Camera* camera_component =
        cam_obj->get_component<Camera>("camera_component");

    camera_component->move_to(x + 0.5f, y + 0.5f);
}
