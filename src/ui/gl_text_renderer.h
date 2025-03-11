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

#ifndef GL_TEXT_RENDERER_H_
#define GL_TEXT_RENDERER_H_

#include <array>

#include "GL/gl.h"

#include "ui/gl_canvas.h"
#include "visualization/shader.h"

namespace oid
{

// std::array<std::array<int, 2>, 256> is int[256][2]
using Array_256_2 = std::array<std::array<int, 2>, 256>;

class GLTextRenderer
{
  public:
    QFont font{"Times New Roman", 96};
    GLuint text_vbo{0};
    GLuint text_tex{0};

    Array_256_2 text_texture_offsets{};
    Array_256_2 text_texture_advances{};
    Array_256_2 text_texture_sizes{};
    Array_256_2 text_texture_tls{};

    explicit GLTextRenderer(GLCanvas* gl_canvas);
    ~GLTextRenderer();

    bool initialize();

    void generate_glyphs_texture();

    ShaderProgram text_prog;

    float text_texture_width{0};
    float text_texture_height{0};

  private:
    GLCanvas* gl_canvas_{};
};

} // namespace oid

#endif // GL_TEXT_RENDERER_H_
