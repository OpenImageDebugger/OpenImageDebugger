#include "component.h"

bool Component::initialize() {
    return true;
}

bool Component::buffer_update() {
    return true;
}

bool Component::post_buffer_update() {
    return true;
}

int Component::render_index() const {
    return 0;
}

void Component::mouse_drag_event(int, int) {}

bool Component::post_initialize() {
    return true;
}

Component::~Component() {}
