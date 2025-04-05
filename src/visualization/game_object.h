/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2024 OpenImageDebugger contributors
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

#ifndef GAME_OBJECT_H_
#define GAME_OBJECT_H_

#include <functional>
#include <memory>

#include "events.h"
#include "math/linear_algebra.h"
#include "stage.h"


class GLCanvas;

namespace oid
{

class GameObject
{
  public:
    Stage* stage{nullptr};

    GameObject();

    template <typename T>
    T* get_component(const std::string& tag)
    {
        if (all_components_.find(tag) == all_components_.end()) {
            return nullptr;
        }
        return dynamic_cast<T*>(all_components_[tag].get());
    }

    [[nodiscard]] bool initialize() const;

    [[nodiscard]] bool post_initialize() const;

    void update() const;

    void add_component(const std::string& component_name,
                       const std::shared_ptr<Component>& component);

    mat4 get_pose();

    void set_pose(const mat4& pose);

    void request_render_update() const;

    void mouse_drag_event(int mouse_x, int mouse_y) const;

    void mouse_move_event(int mouse_x, int mouse_y) const;

    [[nodiscard]] EventProcessCode key_press_event(int key_code) const;

    const std::map<std::string, std::shared_ptr<Component>, std::less<>>&
    get_components() const;

  private:
    std::map<std::string, std::shared_ptr<Component>, std::less<>>
        all_components_;
    mat4 pose_;
};

} // namespace oid
#endif // GAME_OBJECT_H_
