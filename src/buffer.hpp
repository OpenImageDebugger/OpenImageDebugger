#pragma once

#include <vector>
#include "shader.hpp"
#include "component.hpp"

using namespace std;
class Buffer : public Component {
public:
    int max_texture_size = 2048;

    std::vector<GLuint> buff_tex;

    ~Buffer() {
        int num_textures = num_textures_x*num_textures_y;
        glDeleteTextures(num_textures, buff_tex.data());
        glDeleteBuffers(1, &vbo);
    }

    float auto_buffer_contrast_brightness[6] = {1.0,1.0,1.0, 0.0, 0.0, 0.0};
    float buffer_width_f;
    float buffer_height_f;
    int channels;
    int type;
    int step;
    uint8_t* buffer;

    bool buffer_update() {
        int num_textures = num_textures_x*num_textures_y;
        glDeleteTextures(num_textures, buff_tex.data());

        setup_gl_buffer();
        return true;
    }

    void computeContrastBrightnessParameters() {
        int buffer_width_i = static_cast<int>(buffer_width_f);
        int buffer_height_i = static_cast<int>(buffer_height_f);

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

        for(int y = 0; y < buffer_height_i; ++y) {
            for(int x = 0; x < buffer_width_i; ++x) {
                int i = y*step + x;
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
        }

        float* auto_buffer_contrast = auto_buffer_contrast_brightness;
        float* auto_buffer_brightness = auto_buffer_contrast_brightness+3;
        for(int c = 0; c < channels; ++c) {
            float maxIntensity = 1.0f;
            if(type == 0)
                maxIntensity = 1.0f;
            else if(type == 1)
                maxIntensity = 255.0f;
            float upp_minus_low = upper[c]-lowest[c];

            if(upp_minus_low == 0)
                upp_minus_low = 1.0;

            auto_buffer_contrast[c] = maxIntensity/upp_minus_low;
            auto_buffer_brightness[c] = -lowest[c]/maxIntensity*auto_buffer_contrast[c];
        }
        if(channels != 3) {
            for(int c = channels; c < 3; ++c) {
                auto_buffer_contrast[c] = auto_buffer_contrast[0];
                auto_buffer_brightness[c] = auto_buffer_brightness[0];
            }
        }
    }

    int sub_texture_id_at_coord(int x, int y) {
        int tx = x/max_texture_size;
        int ty = y/max_texture_size;
        return buff_tex[ty*num_textures_x + tx];
    }

    float tile_coord_x(int x) {
        int buffer_width_i = static_cast<int>(buffer_width_f);
        int last_width = buffer_width_i%max_texture_size;
        float tile_width = (x > (buffer_width_i-last_width))
                           ? last_width : max_texture_size;
        return static_cast<float>(x%max_texture_size)/static_cast<float>(tile_width-1);
    }
    float tile_coord_y(int y) {
        int buffer_height_i = static_cast<int>(buffer_height_f);
        int last_height = buffer_height_i%max_texture_size;
        int tile_height =  (y > (buffer_height_i-last_height))
                           ? last_height : max_texture_size;
        return static_cast<float>(y%max_texture_size)/static_cast<float>(tile_height-1);
    }

    bool initialize() {
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
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f,  1.0f,
            1.0f,  1.0f,
            0.0f,  1.0f,
            0.0f, 0.0f,
        };

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

        setup_gl_buffer();

        return true;
    }

    void update();

    void draw(const mat4& projection, const mat4& viewInv) {
        buff_prog.use();
        glEnableVertexAttribArray(0);
        glActiveTexture(GL_TEXTURE0);
        buff_prog.uniform1i("sampler", 0);
        buff_prog.uniform3fv("brightness_contrast", 2, auto_buffer_contrast_brightness);

        int buffer_width_i = static_cast<int>(buffer_width_f);
        int buffer_height_i = static_cast<int>(buffer_height_f);

        int remaining_h = buffer_height_i;
        for(int ty = 0; ty < num_textures_y; ++ty) {
            int buff_h = std::min(remaining_h, max_texture_size);
            remaining_h -= buff_h;

            int remaining_w = buffer_width_i;
            for(int tx = 0; tx < num_textures_x; ++tx) {
                int buff_w = std::min(remaining_w, max_texture_size);
                remaining_w -= buff_w;

                glBindTexture(GL_TEXTURE_2D, buff_tex[ty*num_textures_x+tx]);

                mat4 tile_model;
                tile_model.setFromST(buff_w, buff_h, 1.0,
                        posX()+tx*max_texture_size,
                        posY()+ty*max_texture_size, 0.0f);
                buff_prog.uniformMatrix4fv("mvp", 1, GL_FALSE, (projection * viewInv * tile_model).data);
                buff_prog.uniform2f("buffer_dimension", buff_w, buff_h);

                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
    }

    int num_textures_x;
    int num_textures_y;

private:
    void setup_gl_buffer() {
        int buffer_width_i = static_cast<int>(buffer_width_f);
        int buffer_height_i = static_cast<int>(buffer_height_f);

        model.setFromST(buffer_width_i, buffer_height_i, 1.0, 0.0, 0.0, 0.0f);

        // Initialize contrast parameters
        computeContrastBrightnessParameters();

        // Buffer texture
        num_textures_x = ceil(((float)buffer_width_i)/((float)max_texture_size));
        num_textures_y = ceil(((float)buffer_height_i)/((float)max_texture_size));
        int num_textures = num_textures_x*num_textures_y;

        buff_tex.resize(num_textures);
        glGenTextures(num_textures, buff_tex.data());

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

        int remaining_h = buffer_height_i;
        glPixelStoref(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, step);

        for(int ty = 0; ty < num_textures_y; ++ty) {
            int buff_h = std::min(remaining_h, max_texture_size);
            remaining_h -= buff_h;

            int remaining_w = buffer_width_i;
            for(int tx = 0; tx < num_textures_x; ++tx) {
                int buff_w = std::min(remaining_w, max_texture_size);
                remaining_w -= buff_w;

                int tex_id = ty*num_textures_x + tx;
                glBindTexture(GL_TEXTURE_2D, buff_tex[tex_id]);

                glPixelStorei(GL_UNPACK_SKIP_ROWS, ty*max_texture_size);
                glPixelStorei(GL_UNPACK_SKIP_PIXELS, tx*max_texture_size);
                glTexStorage2D( GL_TEXTURE_2D, 1, GL_RGB32F, buff_w, buff_h);

                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        buff_w, buff_h, tex_format, tex_type,
                        reinterpret_cast<GLvoid*>(buffer));

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            }
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    }

    ShaderProgram buff_prog;
    GLuint vbo;
};

