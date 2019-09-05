/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 OpenImageDebugger contributors
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

#include <cmath>
#include <map>

#include "game_object.h"

#include "ui/main_window/main_window.h"
#include "visualization/components/camera.h"
#include "visualization/components/component.h"
#include "visualization/stage.h"


GameObject::GameObject()
{
    pose_.set_identity();
}


bool GameObject::initialize()
{
    for (const auto& comp : all_components_) {
        if (!comp.second->initialize()) {
            return false;
        }
    }
    return true;
}


bool GameObject::post_initialize()
{
    for (const auto& comp : all_components_) {
        if (!comp.second->post_initialize()) {
            return false;
        }
    }
    return true;
}


void GameObject::update()
{
    for (const auto& comp : all_components_) {
        comp.second->update();
    }
}

void GameObject::add_component(const std::string& component_name,
                               std::shared_ptr<Component> component)
{
    all_components_[component_name] = component;
}


mat4 GameObject::get_pose()
{
    return pose_;
}


void GameObject::set_pose(const mat4& pose)
{
    pose_ = pose;
}


void GameObject::request_render_update()
{
    stage->main_window->request_render_update();
}


void GameObject::mouse_drag_event(int mouse_x, int mouse_y)
{
    for (const auto& comp : all_components_) {
        comp.second->mouse_drag_event(mouse_x, mouse_y);
    }
}


void GameObject::mouse_move_event(int mouse_x, int mouse_y)
{
    for (const auto& comp : all_components_) {
        comp.second->mouse_move_event(mouse_x, mouse_y);
    }
}


EventProcessCode GameObject::key_press_event(int key_code)
{
    EventProcessCode event_intercepted = EventProcessCode::IGNORED;

    for (const auto& comp : all_components_) {
        EventProcessCode event_intercepted_component =
            comp.second->key_press_event(key_code);

        if (event_intercepted_component == EventProcessCode::INTERCEPTED) {
            event_intercepted = EventProcessCode::INTERCEPTED;
        }
    }

    return event_intercepted;
}


const std::map<std::string, std::shared_ptr<Component>>&
GameObject::get_components()
{
    return all_components_;
}
