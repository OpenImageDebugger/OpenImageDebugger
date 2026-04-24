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

#ifndef COMPONENT_H_
#define COMPONENT_H_

#include <memory>

#include "math/linear_algebra.h"
#include "ui/gl_canvas.h"
#include "visualization/events.h"


namespace oid
{

class GameObject;

class Component
{
  public:
    Component(std::shared_ptr<GameObject> game_object,
              std::shared_ptr<GLCanvas> gl_canvas);

    [[nodiscard]] virtual bool initialize();

    [[nodiscard]] virtual bool buffer_update();

    [[nodiscard]] virtual bool post_buffer_update();

    [[nodiscard]] virtual int render_index() const;

    // Called after all components are initialized
    [[nodiscard]] virtual bool post_initialize();

    virtual void update() = 0;

    virtual void draw(const mat4& projection, const mat4& viewInv) = 0;

    ///
    // Events
    virtual EventProcessCode key_press_event(int /* key_code */)
    {
        return EventProcessCode::IGNORED;
    }

    virtual void mouse_drag_event(int /* mouse_x */, int /* mouse_y */)
    {
        // Do nothing
    }

    virtual void mouse_move_event(int /* mouse_x */, int /* mouse_y */)
    {
        // Do nothing
    }

    virtual ~Component() noexcept;

    [[nodiscard]] std::shared_ptr<GameObject> game_object() const noexcept
    {
        return game_object_.lock();
    }

    [[nodiscard]] GameObject& game_object_ref() const
    {
        auto obj = game_object_.lock();
        assert(obj && "GameObject has been destroyed");
        return *obj;
    }

    [[nodiscard]] std::shared_ptr<GLCanvas> gl_canvas() const noexcept
    {
        return gl_canvas_.lock();
    }

    [[nodiscard]] GLCanvas& gl_canvas_ref() const
    {
        auto canvas = gl_canvas_.lock();
        assert(canvas && "GLCanvas has been destroyed");
        return *canvas;
    }

  public:
    std::weak_ptr<GameObject> game_object_;

    std::weak_ptr<GLCanvas> gl_canvas_;
};

} // namespace oid
#endif // COMPONENT_H_
