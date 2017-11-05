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

#ifndef COMPONENT_H_
#define COMPONENT_H_


class GameObject;
class GLCanvas;
class mat4;


class Component
{
  public:
    Component(GameObject* game_object, GLCanvas* gl_canvas);

    virtual bool initialize();

    virtual bool buffer_update();

    virtual bool post_buffer_update();

    virtual int render_index() const;

    virtual void mouse_drag_event(int mouse_x, int mouse_y);

    virtual void mouse_move_event(int mouse_x, int mouse_y);

    // Called after all components are initialized
    virtual bool post_initialize();

    virtual void update() = 0;

    virtual void draw(const mat4& projection, const mat4& viewInv) = 0;

    virtual ~Component();

  protected:
    GameObject* game_object_;

    GLCanvas* gl_canvas_;
};

#endif // COMPONENT_H_
