#include <GL/glew.h>
#include "shader.hpp"

bool ShaderProgram::create(const char* v_source, const char* f_source, TexelChannels texel_format, const std::vector<std::string>& uniforms) {
    if(program_ != 0) {
        // Delete old program
        glDeleteProgram(program_);
    }

    this->texel_format_ = texel_format;
    GLuint vertex_shader = compile(GL_VERTEX_SHADER, v_source);
    GLuint fragment_shader = compile(GL_FRAGMENT_SHADER, f_source);

    if(vertex_shader == 0 || fragment_shader == 0)
        return false;

    program_ = glCreateProgram();
    glAttachShader(program_, vertex_shader);
    glAttachShader(program_, fragment_shader);
    glLinkProgram(program_);

    // Delete shaders. We don't need them anymore.
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // Get uniform locations
    for(const auto& name: uniforms) {
        GLuint loc = glGetUniformLocation(program_, name.c_str());
        uniforms_[name] = loc;
    }

    return true;
}

void ShaderProgram::uniform1i(const std::string& name, int value) {
    glUniform1i(uniforms_[name], value);
}

void ShaderProgram::uniform2f(const std::string& name, float x, float y) {
    glUniform2f(uniforms_[name], x, y);
}

void ShaderProgram::uniform3fv(const std::string& name, int count, const float* data) {
    glUniform3fv(uniforms_[name], count, data);
}

void ShaderProgram::uniform4fv(const std::string& name, int count, const float* data) {
    glUniform4fv(uniforms_[name], count, data);
}

void ShaderProgram::uniformMatrix4fv(const std::string& name, int count, GLboolean transpose, const float* value) {
    glUniformMatrix4fv(uniforms_[name], count, transpose, value);
}

void ShaderProgram::use() {
    glUseProgram(program_);
}

ShaderProgram::~ShaderProgram() {
    glDeleteProgram(program_);
}

GLuint ShaderProgram::compile(GLuint type, GLchar const *source) {
    GLuint shader = glCreateShader(type);
    const char* src[] = {
        "#version 120\n",
        texel_format_== FormatR ?
          "#define FORMAT_R\n"
        : texel_format_ == FormatRG ?
          "#define FORMAT_RG"
        : "",
        source
    };
    glShaderSource(shader, 3, src, NULL);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(length, ' ');
        glGetShaderInfoLog(shader, length, &length, &log[0]);
        std::cerr << "Failed to compile shadertype: "+ getShaderType(type) << std::endl
                  << log << std::endl;
        return false;
    }
    return shader;
}

std::string ShaderProgram::getShaderType(GLuint type){
    std::string name;
    switch(type){
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
