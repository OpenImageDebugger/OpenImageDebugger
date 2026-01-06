/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
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

#ifndef SHADER_H_
#define SHADER_H_

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "GL/gl.h"

#include "ui/gl_canvas.h"

namespace oid
{

class ShaderProgram
{
  public:
    enum class TexelChannels { FormatR, FormatRG, FormatRGB, FormatRGBA };

    explicit ShaderProgram(GLCanvas* gl_canvas);

    ShaderProgram(const ShaderProgram&) = delete;

    ShaderProgram(ShaderProgram&&) = delete;

    ShaderProgram& operator=(const ShaderProgram&) = delete;

    ShaderProgram& operator=(ShaderProgram&&) = delete;

    ~ShaderProgram();

    bool create(const char* v_source,
                const char* f_source,
                TexelChannels texel_format,
                const std::string& pixel_layout,
                const std::vector<std::string>& uniforms);

    // Uniform handlers
    void uniform1i(const std::string& name, int value) const;

    void uniform2f(const std::string& name, float x, float y) const;

    void
    uniform3fv(const std::string& name, int count, const float* data) const;

    void
    uniform4fv(const std::string& name, int count, const float* data) const;

    void uniform_matrix4fv(const std::string& name,
                           int count,
                           GLboolean transpose,
                           const float* value) const;

    // Program utility
    void use() const;

  private:
    GLuint program_{0};

    GLCanvas* gl_canvas_{};

    TexelChannels texel_format_{};

    std::map<std::string, GLuint, std::less<>> uniforms_{};

    std::string pixel_layout_{};

    GLuint compile(GLuint type, GLchar const* source) const;

    static std::string get_shader_type(GLuint type);

    bool is_shader_outdated(TexelChannels texel_format,
                            const std::vector<std::string>& uniforms,
                            const std::string& pixel_layout) const;

    const char* get_texel_format_define() const;
};

} // namespace oid

#endif // SHADER_H_
