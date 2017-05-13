#include <GL/glew.h>
#include "background.hpp"
#include "stage.hpp"

Background::~Background() {
    glDeleteBuffers(1, &background_vbo);
}

bool Background::initialize() {
    background_prog.create(shader::background_vert_shader,
                           shader::background_frag_shader,
                           ShaderProgram::FormatR, {});

    // Generate square VBO
    static const GLfloat vertex_buffer_data[] = {
        -1, -1,
         1, -1,
         1,  1,
         1,  1,
        -1,  1,
        -1, -1,
    };
    glGenBuffers(1, &background_vbo);

    glBindBuffer(GL_ARRAY_BUFFER, background_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

    return true;
}

void Background::draw(const mat4& projection, const mat4& viewInv) {
    background_prog.use();

    mat4 model = game_object->get_pose();
    mat4 mvp = projection * viewInv * model;

    background_prog.uniformMatrix4fv("mvp", 1, GL_FALSE, mvp.data());
    background_prog.uniform2f("screen_dimension", 100, 100);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, background_vbo);
    glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

int Background::render_index() const {
    return -100;
}
