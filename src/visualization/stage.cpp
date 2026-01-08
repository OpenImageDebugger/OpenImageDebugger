/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
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

#include "stage.h"

#include <algorithm>
#include <iostream>
#include <ranges>
#include <set>
#include <string>

#include "game_object.h"
#include "ui/main_window/main_window.h"
#include "visualization/components/background.h"
#include "visualization/components/buffer_values.h"
#include "visualization/components/camera.h"


namespace oid
{

namespace
{

// Helper function to safely get the camera component
Camera* get_camera_component(
    const std::map<std::string, std::shared_ptr<GameObject>, std::less<>>&
        game_objects)
{
    const auto camera_it = game_objects.find("camera");
    if (camera_it == game_objects.end() || !camera_it->second) {
        std::cerr << "[Error] Camera game object not found" << std::endl;
        return nullptr;
    }

    const auto camera_component =
        camera_it->second->get_component<Camera>("camera_component");

    if (camera_component == nullptr) {
        std::cerr << "[Error] Camera component not found" << std::endl;
        return nullptr;
    }

    return camera_component;
}

} // namespace

struct CompareRenderOrder
{
    bool operator()(const Component* a, const Component* b) const
    {
        return a->render_index() < b->render_index();
    }
};


Stage::Stage(MainWindow* main_window)
    : main_window_{main_window}
{
}


bool Stage::get_contrast_enabled() const
{
    return contrast_enabled_;
}


void Stage::set_contrast_enabled(const bool enabled)
{
    contrast_enabled_ = enabled;
}


std::vector<uint8_t>& Stage::get_buffer_icon()
{
    return buffer_icon_;
}


const std::vector<uint8_t>& Stage::get_buffer_icon() const
{
    return buffer_icon_;
}


MainWindow* Stage::get_main_window() const
{
    return main_window_;
}


bool Stage::initialize(const BufferParams& params)
{
    const auto camera_obj = std::make_shared<GameObject>();

    camera_obj->set_stage(this);

    camera_obj->add_component(
        "camera_component",
        std::make_shared<Camera>(camera_obj.get(), main_window_->gl_canvas()));
    camera_obj->add_component("background_component",
                              std::make_shared<Background>(
                                  camera_obj.get(), main_window_->gl_canvas()));

    all_game_objects["camera"] = camera_obj;

    const auto buffer_obj = std::make_shared<GameObject>();

    buffer_obj->set_stage(this);

    buffer_obj->add_component("text_component",
                              std::make_shared<BufferValues>(
                                  buffer_obj.get(), main_window_->gl_canvas()));

    const auto buffer_component =
        std::make_shared<Buffer>(buffer_obj.get(), main_window_->gl_canvas());

    buffer_component->configure(params);
    buffer_obj->add_component("buffer_component", buffer_component);

    all_game_objects["buffer"] = buffer_obj;

    for (const auto& go : all_game_objects | std::views::values) {
        if (!go->initialize()) {
            return false;
        }
    }

    return std::ranges::all_of(all_game_objects, [](auto& go) {
        return go.second->post_initialize();
    });
}


bool Stage::buffer_update(const BufferParams& params)
{
    const auto buffer_it = all_game_objects.find("buffer");
    if (buffer_it == all_game_objects.end() || !buffer_it->second) {
        std::cerr << "[Error] Buffer game object not found" << std::endl;
        return false;
    }

    const auto buffer_obj = buffer_it->second.get();
    const auto buffer_component =
        buffer_obj->get_component<Buffer>("buffer_component");

    if (buffer_component == nullptr) {
        std::cerr << "[Error] Buffer component not found" << std::endl;
        return false;
    }

    buffer_component->configure(params);

    for (const auto& game_obj_it : all_game_objects | std::views::values) {
        const auto game_obj = game_obj_it.get();
        game_obj->set_stage(this);
        for (const auto& comp :
             game_obj->get_components() | std::views::values) {
            if (!comp->buffer_update()) {
                return false;
            }
        }
    }

    for (const auto& game_obj_it : all_game_objects | std::views::values) {
        for (const auto& comp :
             game_obj_it.get()->get_components() | std::views::values) {
            if (!comp->post_buffer_update()) {
                return false;
            }
        }
    }

    return true;
}


GameObject* Stage::get_game_object(const std::string& tag)
{
    if (!all_game_objects.contains(tag)) {
        return nullptr;
    }

    return all_game_objects[tag].get();
}


void Stage::update() const
{
    for (const auto& game_obj : all_game_objects | std::views::values) {
        game_obj->update();
    }
}


void Stage::draw()
{
    auto ordered_components = std::set<Component*, CompareRenderOrder>{};

    // TODO: use camera tags so I can have multiple cameras (useful for drawing
    // GUI)

    const auto camera_it = all_game_objects.find("camera");
    if (camera_it == all_game_objects.end() || !camera_it->second) {
        std::cerr << "[Error] Camera game object not found" << std::endl;
        return;
    }

    const auto camera_obj = camera_it->second.get();
    const auto camera_component =
        camera_obj->get_component<Camera>("camera_component");

    if (camera_component == nullptr) {
        std::cerr << "[Error] Camera component not found" << std::endl;
        return;
    }

    const auto view_inv = camera_obj->get_pose().inv();

    for (const auto& game_obj : all_game_objects | std::views::values) {
        for (const auto& component :
             game_obj->get_components() | std::views::values) {
            ordered_components.insert(component.get());
        }
    }

    for (const auto& component : ordered_components) {
        component->draw(camera_component->projection, view_inv);
    }
}


void Stage::scroll_callback(const float delta) const
{
    const auto camera_component = get_camera_component(all_game_objects);
    if (camera_component == nullptr) {
        return;
    }

    camera_component->scroll_callback(delta);
}


void Stage::resize_callback(const int w, const int h) const
{
    const auto camera_component = get_camera_component(all_game_objects);
    if (camera_component == nullptr) {
        return;
    }

    camera_component->window_resized(w, h);
}


void Stage::mouse_drag_event(const int mouse_x, const int mouse_y) const
{
    for (const auto& game_obj : all_game_objects | std::views::values) {
        game_obj->mouse_drag_event(mouse_x, mouse_y);
    }
}


void Stage::mouse_move_event(const int mouse_x, const int mouse_y) const
{
    for (const auto& game_obj : all_game_objects | std::views::values) {
        game_obj->mouse_move_event(mouse_x, mouse_y);
    }
}


EventProcessCode Stage::key_press_event(const int key_code) const
{
    auto event_intercepted = EventProcessCode::IGNORED;

    for (const auto& game_obj : all_game_objects | std::views::values) {
        const auto event_intercepted_game_obj =
            game_obj->key_press_event(key_code);

        if (event_intercepted_game_obj == EventProcessCode::INTERCEPTED) {
            event_intercepted = EventProcessCode::INTERCEPTED;
        }
    }

    return event_intercepted;
}


void Stage::go_to_pixel(const float x, const float y) const
{
    const auto camera_component = get_camera_component(all_game_objects);
    if (camera_component == nullptr) {
        return;
    }

    camera_component->move_to(x, y);
}


void Stage::set_icon_drawing_mode(const bool is_enabled)
{
    const auto buffer_obj = all_game_objects["buffer"].get();
    if (buffer_obj == nullptr) {
        return;
    }

    const auto buffer_component =
        buffer_obj->get_component<Buffer>("buffer_component");
    if (buffer_component == nullptr) {
        return;
    }

    buffer_component->set_icon_drawing_mode(is_enabled);
}

} // namespace oid
