#ifndef BACKGROUND_HPP
#define BACKGROUND_HPP

#include "shader.hpp"
#include "component.hpp"

class Background : public Component
{
public:
    ~Background();

    bool initialize();

    void update() {}

    void draw(const mat4& projection, const mat4& viewInv);

private:
    ShaderProgram background_prog;
    GLuint background_vbo;
};

#endif // BACKGROUND_HPP
