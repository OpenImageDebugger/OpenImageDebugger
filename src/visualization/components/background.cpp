/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2024 OpenImageDebugger contributors
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

#include "background.h"

#include "math/linear_algebra.h"
#include "visualization/game_object.h"
#include "visualization/shader.h"
#include "visualization/shaders/oid_shaders.h"
#include "visualization/stage.h"


Background::Background(GameObject* game_object, GLCanvas* gl_canvas)
    : Component(game_object, gl_canvas)
    , background_prog(gl_canvas)
    , background_vbo(0)
{
}


Background::~Background()
{
    gl_canvas_->glDeleteBuffers(1, &background_vbo);
}


bool Background::initialize()
{
    background_prog.create(shader::background_vert_shader,
                           shader::background_frag_shader,
                           ShaderProgram::FormatR,
                           "rgba",
                           {});

    // Generate square VBO
    // clang-format off
    static constexpr GLfloat vertex_buffer_data[] = {
        -1, -1,
        1, -1,
        1, 1,
        1, 1,
        -1, 1,
        -1, -1,
    };
    // clang-format on
    gl_canvas_->glGenBuffers(1, &background_vbo);

    gl_canvas_->glBindBuffer(GL_ARRAY_BUFFER, background_vbo);
    gl_canvas_->glBufferData(GL_ARRAY_BUFFER,
                             sizeof(vertex_buffer_data),
                             vertex_buffer_data,
                             GL_STATIC_DRAW);

    return true;
}


void Background::draw(const mat4&, const mat4&)
{
    background_prog.use();

    gl_canvas_->glEnableVertexAttribArray(0);
    gl_canvas_->glBindBuffer(GL_ARRAY_BUFFER, background_vbo);
    gl_canvas_->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    gl_canvas_->glDrawArrays(GL_TRIANGLES, 0, 6);
}


int Background::render_index() const
{
    return -100;
}
