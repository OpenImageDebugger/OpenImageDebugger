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

#ifndef EVENTS_H_
#define EVENTS_H_

#include <set>

enum class EventProcessCode { IGNORED, INTERCEPTED };

class MainWindow;
class QEvent;

class KeyboardState
{
  public:
    friend class MainWindow;

    enum class ModifierKey { Control, Alt, Shift };
    enum class Key { Left, Right, Up, Down, Plus, Minus };

    static bool is_modifier_key_pressed(ModifierKey key);

    static bool is_key_pressed(Key key);

  private:
    static void update_keyboard_state(const QEvent* event);

    static std::set<int> pressed_keys_;
};

#endif // EVENTS_H_
