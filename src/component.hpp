#pragma once
#include "math.hpp"

class Stage;
class Component {
public:
    Stage* stage;
    mat4 model;
    virtual bool initialize() {
        return true;
    }
    virtual bool buffer_update() {
        return true;
    }
    virtual bool post_buffer_update() {
        return true;
    }

    // Called after all components are initialized
    virtual bool post_initialize() {
        return true;
    }
    virtual void update() = 0;
    virtual void draw(const mat4& projection, const mat4& viewInv) = 0;
    float posX() {
        return model.data[12];
    }
    float posY() {
        return model.data[13];
    }
    virtual ~Component(){}
};

