/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2025 OpenImageDebugger contributors
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

#include "events.h"

#include <cassert>

#include <QApplication>
#include <QKeyEvent>

namespace oid
{

std::set<int> KeyboardState::pressed_keys_{};


bool KeyboardState::is_modifier_key_pressed(const ModifierKey key)
{
    switch (key) {
    case ModifierKey::Alt:
        return (QApplication::keyboardModifiers() & Qt::AltModifier) != 0;
    case ModifierKey::Control:
        return (QApplication::keyboardModifiers() & Qt::ControlModifier) != 0;
    case ModifierKey::Shift:
        return (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
    default:
        assert(!"Invalid modifier key");
        return false;
    }
}


bool KeyboardState::is_key_pressed(const Key key)
{
    switch (key) {
    case Key::Left:
        return pressed_keys_.contains(Qt::Key_Left);
    case Key::Right:
        return pressed_keys_.contains(Qt::Key_Right);
    case Key::Up:
        return pressed_keys_.contains(Qt::Key_Up);
    case Key::Down:
        return pressed_keys_.contains(Qt::Key_Down);
    case Key::Plus:
        return pressed_keys_.contains(Qt::Key_Plus);
    case Key::Minus:
        return pressed_keys_.contains(Qt::Key_Minus);
    default:
        assert(!"Invalid key requested");
        return false;
    }
}


void KeyboardState::update_keyboard_state(const QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        pressed_keys_.insert(dynamic_cast<const QKeyEvent*>(event)->key());
    } else if (event->type() == QEvent::KeyRelease) {
        pressed_keys_.erase(dynamic_cast<const QKeyEvent*>(event)->key());
    }
}

} // namespace oid
