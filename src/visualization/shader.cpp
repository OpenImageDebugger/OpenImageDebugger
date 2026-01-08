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

#include "shader.h"

#include <cstring>

#include <iostream>

namespace oid
{

ShaderProgram::ShaderProgram(GLCanvas& gl_canvas)
    : gl_canvas_{gl_canvas}
{
}


ShaderProgram::~ShaderProgram()
{
    gl_canvas_.glDeleteProgram(program_);
}


bool ShaderProgram::is_shader_outdated(const TexelChannels texel_format,
                                       const std::vector<std::string>& uniforms,
                                       const std::string& pixel_layout) const
{
    // If the texel format or the uniform container size changed,
    // the program must be created again
    if (texel_format != texel_format_ || uniforms.size() != uniforms_.size()) {
        return true;
    }

    // The program must also be created again if a uniform name
    // changed
    for (const auto& uniform_name : uniforms) {
        if (!uniforms_.contains(uniform_name)) {
            return true;
        }
    }

    for (int i = 0; i < 4; ++i) {
        if (pixel_layout[i] != pixel_layout_[i]) {
            return true;
        }
    }

    // Otherwise, it must not change
    return false;
}

bool ShaderProgram::create(const char* v_source,
                           const char* f_source,
                           const TexelChannels texel_format,
                           const std::string& pixel_layout,
                           const std::vector<std::string>& uniforms)
{
    if (program_ != 0) {
        // Check if the program needs to be recompiled
        if (!is_shader_outdated(texel_format, uniforms, pixel_layout)) {
            return true;
        }
        // Delete old program
        gl_canvas_.glDeleteProgram(program_);
    }

    texel_format_ = texel_format;
    memcpy(pixel_layout_.data(), pixel_layout.data(), 4);
    pixel_layout_[4]           = '\0';
    const auto vertex_shader   = compile(GL_VERTEX_SHADER, v_source);
    const auto fragment_shader = compile(GL_FRAGMENT_SHADER, f_source);

    if (vertex_shader == 0 || fragment_shader == 0) {
        return false;
    }

    program_ = gl_canvas_.glCreateProgram();
    gl_canvas_.glAttachShader(program_, vertex_shader);
    gl_canvas_.glAttachShader(program_, fragment_shader);
    gl_canvas_.glLinkProgram(program_);

    // Delete shaders. We don't need them anymore.
    gl_canvas_.glDeleteShader(vertex_shader);
    gl_canvas_.glDeleteShader(fragment_shader);

    // Check for link errors
    auto linked = GLint{};
    gl_canvas_.glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint length;
        gl_canvas_.glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &length);
        auto log = std::string(length, ' ');
        gl_canvas_.glGetProgramInfoLog(program_, length, &length, &log[0]);
        std::cerr << "[Error] Failed to link shader program:" << std::endl
                  << log << std::endl;
        gl_canvas_.glDeleteProgram(program_);
        program_ = 0;
        return false;
    }

    // Get uniform locations
    for (const auto& name : uniforms) {
        const auto loc =
            gl_canvas_.glGetUniformLocation(program_, name.c_str());
        uniforms_[name] = loc;
    }

    return true;
}


void ShaderProgram::uniform1i(const std::string& name, const int value) const
{
    gl_canvas_.glUniform1i(static_cast<GLint>(uniforms_.at(name)), value);
}


void ShaderProgram::uniform2f(const std::string& name,
                              const float x,
                              const float y) const
{
    gl_canvas_.glUniform2f(static_cast<GLint>(uniforms_.at(name)), x, y);
}


void ShaderProgram::uniform3fv(const std::string& name,
                               const int count,
                               const float* data) const
{
    gl_canvas_.glUniform3fv(
        static_cast<GLint>(uniforms_.at(name)), count, data);
}


void ShaderProgram::uniform4fv(const std::string& name,
                               const int count,
                               const float* data) const
{
    gl_canvas_.glUniform4fv(
        static_cast<GLint>(uniforms_.at(name)), count, data);
}


void ShaderProgram::uniform_matrix4fv(const std::string& name,
                                      const int count,
                                      const GLboolean transpose,
                                      const float* value) const
{
    gl_canvas_.glUniformMatrix4fv(
        static_cast<GLint>(uniforms_.at(name)), count, transpose, value);
}


void ShaderProgram::use() const
{
    gl_canvas_.glUseProgram(program_);
}


const char* ShaderProgram::get_texel_format_define() const
{
    switch (texel_format_) {
    case TexelChannels::FormatR:
        return "#define FORMAT_R\n";
    case TexelChannels::FormatRG:
        return "#define FORMAT_RG\n";
    case TexelChannels::FormatRGB:
        return "#define FORMAT_RGB\n";
    case TexelChannels::FormatRGBA:
        return "#define FORMAT_RGBA\n";
    default:
        return "";
    }
}


GLuint ShaderProgram::compile(const GLuint type, const GLchar* source) const
{
    const auto shader = gl_canvas_.glCreateShader(type);

    auto src = std::array{"#version 120\n",
                          get_texel_format_define(),
                          "#define PIXEL_LAYOUT ",
                          pixel_layout_.data(),
                          source};

    gl_canvas_.glShaderSource(shader, 5, src.data(), nullptr);
    gl_canvas_.glCompileShader(shader);

    auto compiled = GLint{};
    gl_canvas_.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        GLint length;
        gl_canvas_.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        auto log = std::string(length, ' ');
        gl_canvas_.glGetShaderInfoLog(shader, length, &length, &log[0]);
        std::cerr << "Failed to compile shader_type: " + get_shader_type(type)
                  << std::endl
                  << log << std::endl;
        return false;
    }
    return shader;
}


std::string ShaderProgram::get_shader_type(const GLuint type)
{
    auto name = std::string{};
    switch (type) {
    case GL_VERTEX_SHADER:
        name = "Vertex Shader";
        break;
    case GL_FRAGMENT_SHADER:
        name = "Fragment Shader";
        break;
    default:
        name = "Unknown Shader type";
        break;
    }
    return name;
}

} // namespace oid
