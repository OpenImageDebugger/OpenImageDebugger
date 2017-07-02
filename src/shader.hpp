#pragma once

#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <GL/gl.h>

#include "shaders/imagewatch_shaders.hpp"

class ShaderProgram {
public:
    enum TexelChannels {FormatR, FormatRG, FormatRGB, FormatRGBA};

    bool create(const char* v_source, const char* f_source, TexelChannels texel_format, const std::vector<std::string>& uniforms);

    // Uniform handlers
    void uniform1i(const std::string& name, int value);

    void uniform2f(const std::string& name, float x, float y);

    void uniform3fv(const std::string& name, int count, const float *data);

    void uniform4fv(const std::string& name, int count, const float *data);

    void uniformMatrix4fv(const std::string& name, int count, GLboolean transpose, const float* value);

    // Program utility
    void use();

    ~ShaderProgram();

private:
    GLuint program_ = 0;
    TexelChannels texel_format_;
    std::map<std::string, GLuint> uniforms_;

    GLuint compile(GLuint type, GLchar const *source);

    std::string getShaderType(GLuint type);
    bool shaderIsOutdated(TexelChannels texel_format,
                          const std::vector<std::string>& uniforms);
};
