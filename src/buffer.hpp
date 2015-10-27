#pragma once

#include "shader.hpp"
#include "component.hpp"

class Buffer : public Component {
public:
    GLuint buff_tex;

    ~Buffer() {
        glDeleteTextures(1, &buff_tex);
        glDeleteBuffers(1, &vbo);
    }

    float auto_buffer_contrast_brightness[6] = {1.0,1.0,1.0, 0.0, 0.0, 0.0};
    float buffer_width_f;
    float buffer_height_f;
    int channels;
    int type;
    uint8_t* buffer;

    bool initialize() {
        int buffer_width_i = static_cast<int>(buffer_width_f);
        int buffer_height_i = static_cast<int>(buffer_height_f);

        float buffPosX, buffPosY;
        buffPosX = trunc(buffer_width_f/2.0);
        buffPosY = trunc(-buffer_height_f/2.0);
        if(buffer_width_i%2 == 0)
            buffPosX -= 0.5f;
        if(buffer_height_i%2 == 0)
            buffPosY += 0.5f;

        model.setFromST(buffer_width_i, buffer_height_i, 1.0,
                buffPosX, buffPosY, 0.0f);

        // Initialize contrast parameters
        {
            float lowest[] = {
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max()
            };
            float upper[] = {
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest()
            };

            for(int i = buffer_height_i * buffer_width_i -1; i>=0; --i) {
                for(int c = 0; c < channels; ++c) {
                    if(type == 0) {
                        lowest[c] = std::min(lowest[c],
                                reinterpret_cast<float*>(buffer)[channels*i + c]);
                        upper[c] = std::max(upper[c],
                                reinterpret_cast<float*>(buffer)[channels*i + c]);
                    }
                    else if(type == 1) {
                        lowest[c] = std::min(lowest[c],
                                static_cast<float>(buffer[channels*i + c]));
                        upper[c] = std::max(upper[c],
                                static_cast<float>(buffer[channels*i + c]));
                    }
                }
            }

            float* auto_buffer_contrast = auto_buffer_contrast_brightness;
            float* auto_buffer_brightness = auto_buffer_contrast_brightness+3;
            for(int c = 0; c < channels; ++c) {
                float maxIntensity = 1.0f;
                if(type == 0)
                    maxIntensity = 1.0f;
                else if(type == 1)
                    maxIntensity = 255.0f;
                auto_buffer_contrast[c] = maxIntensity/(upper[c]-lowest[c]);
                auto_buffer_brightness[c] = -lowest[c]*auto_buffer_contrast[c];
            }
            if(channels == 1) {
                for(int c = 1; c < 3; ++c) {
                    auto_buffer_contrast[c] = auto_buffer_contrast[0];
                    auto_buffer_brightness[c] = auto_buffer_brightness[0];
                }
            }
        }

        // Buffer Shaders
        ShaderProgram::TexelChannels channelType;
        if(channels == 1)
            channelType = ShaderProgram::Grayscale;
        else
            channelType = ShaderProgram::RGB;

        buff_prog.create(shader::buff_vert_shader,
                shader::buff_frag_shader,
                channelType, { "mvp",
                "sampler", "brightness_contrast",
                "buffer_dimension", "enable_borders"});

        // Buffer VBO
        static const GLfloat g_vertex_buffer_data[] = { 
            -0.5f, -0.5f,
            0.5f, -0.5f,
            0.5f,  0.5f,
            0.5f,  0.5f,
            -0.5f,  0.5f,
            -0.5f, -0.5f,
        };

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

        // Buffer texture
        glGenTextures(1, &buff_tex);
        glBindTexture(GL_TEXTURE_2D, buff_tex);
        GLuint tex_type = GL_UNSIGNED_BYTE;
        GLuint tex_format = GL_RED;
        if(type == 0) {
            tex_type = GL_FLOAT;
        } else if (type == 1) {
            tex_type = GL_UNSIGNED_BYTE;
        }
        if(channels == 1) {
            tex_format = GL_RED;
        } else if(channels == 3) {
            tex_format = GL_RGB;
        }
        glPixelStoref(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexStorage2D( GL_TEXTURE_2D, 1, GL_RGB32F, buffer_width_i, buffer_height_i);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, buffer_width_i, buffer_height_i, tex_format, tex_type, reinterpret_cast<GLvoid*>(buffer));

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        return true;
    }

    void update();

    void draw(const mat4& projection, const mat4& viewInv) {
        buff_prog.use();
        glEnableVertexAttribArray(0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, buff_tex);
        buff_prog.uniform1i("sampler", 0);
        buff_prog.uniform3fv("brightness_contrast", 2, auto_buffer_contrast_brightness);

        buff_prog.uniformMatrix4fv("mvp", 1, GL_FALSE, (projection * viewInv * model).data);
        buff_prog.uniform2f("buffer_dimension", buffer_width_f, buffer_height_f);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

private:
    ShaderProgram buff_prog;
    GLuint vbo;
};

