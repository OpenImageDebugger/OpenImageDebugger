#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <iostream>
#include <map>
#include <vector>

#include "shaders/imagewatch_shaders.hpp"

class ShaderProgram {
public:
    enum TexelChannels {Grayscale, RGB};

    bool create(const char* v_source, const char* f_source, TexelChannels texel_format, const std::vector<std::string>& uniforms) {
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

    // Uniform handlers
    void uniform1i(const std::string& name, int value) {
        glUniform1i(uniforms_[name], value);
    }

    void uniform2f(const std::string& name, float x, float y) {
        glUniform2f(uniforms_[name], x, y);
    }

    void uniform3fv(const std::string& name, int count, float* data) {
        glUniform3fv(uniforms_[name], count, data);
    }

    void uniformMatrix4fv(const std::string& name, int count, GLboolean transpose, const float* value) {
        glUniformMatrix4fv(uniforms_[name], count, transpose, value);
    }

    // Program utility
    void use() {
        glUseProgram(program_);
    }

    ~ShaderProgram() {
        glDeleteProgram(program_);
    }

private:
    GLuint program_ = 0;
    TexelChannels texel_format_;
    std::map<std::string, GLuint> uniforms_;

    GLuint compile(GLuint type, GLchar const *source) {
        GLuint shader = glCreateShader(type);
        const char* src[] = {
            "#version 400 core\n",
            texel_format_==Grayscale?"#define GRAYSCALE\n":"",
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

    std::string getShaderType(GLuint type){
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
};
