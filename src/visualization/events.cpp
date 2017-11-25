#include <QApplication>

#include "events.h"


bool KeyboardState::is_key_pressed(ModifierKey key)
{
    switch (key) {
    case ModifierKey::Alt:
        return (QApplication::keyboardModifiers() & Qt::AltModifier) != 0;
    case ModifierKey::Control:
        return (QApplication::keyboardModifiers() & Qt::ControlModifier) != 0;
    case ModifierKey::Shift:
        return (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
    }
}
