#ifndef BACKGROUND_HPP
#define BACKGROUND_HPP

#include "component.h"
#include "visualization/shader.h"

class Background : public Component
{
public:
    ~Background();

    bool initialize();

    void update() {}

    void draw(const mat4& projection, const mat4& viewInv);

    int render_index() const;

private:
    ShaderProgram background_prog;
    GLuint background_vbo;
};

#endif // BACKGROUND_HPP
