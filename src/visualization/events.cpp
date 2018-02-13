#include <cassert>

#include <QApplication>
#include <QKeyEvent>

#include "events.h"


std::set<int> KeyboardState::pressed_keys_;


bool KeyboardState::is_modifier_key_pressed(ModifierKey key)
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


bool KeyboardState::is_key_pressed(Key key)
{
    switch (key) {
    case Key::Left:
        return pressed_keys_.find(Qt::Key_Left) != pressed_keys_.end();
    case Key::Right:
        return pressed_keys_.find(Qt::Key_Right) != pressed_keys_.end();
    case Key::Up:
        return pressed_keys_.find(Qt::Key_Up) != pressed_keys_.end();
    case Key::Down:
        return pressed_keys_.find(Qt::Key_Down) != pressed_keys_.end();
    case Key::Plus:
        return pressed_keys_.find(Qt::Key_Plus) != pressed_keys_.end();
    case Key::Minus:
        return pressed_keys_.find(Qt::Key_Minus) != pressed_keys_.end();
    default:
        assert(!"Invalid key requested");
        return false;
    }
}


void KeyboardState::update_keyboard_state(const QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        pressed_keys_.insert(static_cast<const QKeyEvent*>(event)->key());
    } else if (event->type() == QEvent::KeyRelease) {
        pressed_keys_.erase(static_cast<const QKeyEvent*>(event)->key());
    }
}
