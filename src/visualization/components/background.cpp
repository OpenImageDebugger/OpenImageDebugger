#include <GL/glew.h>

#include "background.h"

#include "visualization/game_object.h"
#include "visualization/shader.h"
#include "visualization/stage.h"
#include "math/linear_algebra.h"
#include "visualization/shaders/giw_shaders.h"

Background::~Background() {
    glDeleteBuffers(1, &background_vbo);
}

bool Background::initialize() {
    background_prog.create(shader::background_vert_shader,
                           shader::background_frag_shader,
                           ShaderProgram::FormatR,
                           "rgba", {});

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

void Background::draw(const mat4&,
                      const mat4&) {
    glClear(GL_COLOR_BUFFER_BIT);
    background_prog.use();

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, background_vbo);
    glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

int Background::render_index() const {
    return -100;
}
