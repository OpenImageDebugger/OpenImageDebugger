/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 GDB ImageWatch contributors
 * (github.com/csantosbh/gdb-imagewatch/)
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

#include <limits>

#include <GL/glew.h>

#include "buffer.h"

#include "camera.h"
#include "visualization/game_object.h"
#include "visualization/shaders/giw_shaders.h"
#include "visualization/stage.h"


using namespace std;


const float Buffer::no_ac_params[8] = {1.0, 1.0, 1.0, 1.0, 0, 0, 0, 0};


Buffer::~Buffer()
{
    int num_textures = num_textures_x * num_textures_y;

    glDeleteTextures(num_textures, buff_tex.data());
    glDeleteBuffers(1, &vbo);
}


bool Buffer::buffer_update()
{
    int num_textures = num_textures_x * num_textures_y;
    glDeleteTextures(num_textures, buff_tex.data());

    create_shader_program();
    setup_gl_buffer();
    return true;
}


void Buffer::get_pixel_info(stringstream& message, int x, int y)
{
    if (x < 0 || x >= buffer_width_f || y < 0 || y >= buffer_height_f) {
        message << "[out of bounds]";
        return;
    }

    int pos = channels * (y * step + x);

    message << "[";

    for (int c = 0; c < channels; ++c) {
        if (type == Buffer::BufferType::Float32 ||
            type == Buffer::BufferType::Float64) {
            float fpix = reinterpret_cast<float*>(buffer)[pos + c];
            message << fpix;
        } else if (type == Buffer::BufferType::UnsignedByte) {
            short fpix = buffer[pos + c];
            message << fpix;
        } else if (type == Buffer::BufferType::Short) {
            short fpix = reinterpret_cast<short*>(buffer)[pos + c];
            message << fpix;
        } else if (type == Buffer::BufferType::UnsignedShort) {
            unsigned short fpix =
                reinterpret_cast<unsigned short*>(buffer)[pos + c];
            message << fpix;
        } else if (type == Buffer::BufferType::Int32) {
            int fpix = reinterpret_cast<int*>(buffer)[pos + c];
            message << fpix;
        }
        if (c < channels - 1) {
            message << " ";
        }
    }
    message << "]";
}


void Buffer::recompute_min_color_values()
{
    int buffer_width_i  = static_cast<int>(buffer_width_f);
    int buffer_height_i = static_cast<int>(buffer_height_f);

    float* lowest = min_buffer_values();

    for (int i = 0; i < 4; ++i)
        lowest[i] = std::numeric_limits<float>::max();

    for (int y = 0; y < buffer_height_i; ++y) {
        for (int x = 0; x < buffer_width_i; ++x) {
            int i = y * step + x;
            for (int c = 0; c < channels; ++c) {
                if (type == BufferType::Float32 || type == BufferType::Float64)
                    lowest[c] = std::min(
                        lowest[c],
                        reinterpret_cast<float*>(buffer)[channels * i + c]);
                else if (type == BufferType::UnsignedByte)
                    lowest[c] =
                        std::min(lowest[c],
                                 static_cast<float>(buffer[channels * i + c]));
                else if (type == BufferType::Short)
                    lowest[c] =
                        std::min(lowest[c],
                                 static_cast<float>(reinterpret_cast<short*>(
                                     buffer)[channels * i + c]));
                else if (type == BufferType::UnsignedShort)
                    lowest[c] = std::min(
                        lowest[c],
                        static_cast<float>(reinterpret_cast<unsigned short*>(
                            buffer)[channels * i + c]));
                else if (type == BufferType::Int32)
                    lowest[c] =
                        std::min(lowest[c],
                                 static_cast<float>(reinterpret_cast<int*>(
                                     buffer)[channels * i + c]));
            }
        }
    }

    // For single channel buffers: fill with 0
    for (int c = channels; c < 4; ++c)
        lowest[c] = 0.0;
}


void Buffer::recompute_max_color_values()
{
    int buffer_width_i  = static_cast<int>(buffer_width_f);
    int buffer_height_i = static_cast<int>(buffer_height_f);

    float* upper = max_buffer_values();
    for (int i = 0; i < 4; ++i)
        upper[i] = std::numeric_limits<float>::lowest();

    for (int y = 0; y < buffer_height_i; ++y) {
        for (int x = 0; x < buffer_width_i; ++x) {
            int i = y * step + x;
            for (int c = 0; c < channels; ++c) {
                if (type == BufferType::Float32 || type == BufferType::Float64)
                    upper[c] = std::max(
                        upper[c],
                        reinterpret_cast<float*>(buffer)[channels * i + c]);
                else if (type == BufferType::UnsignedByte)
                    upper[c] = std::max(
                        upper[c], static_cast<float>(buffer[channels * i + c]));
                else if (type == BufferType::Short)
                    upper[c] =
                        std::max(upper[c],
                                 static_cast<float>(reinterpret_cast<short*>(
                                     buffer)[channels * i + c]));
                else if (type == BufferType::UnsignedShort)
                    upper[c] = std::max(
                        upper[c],
                        static_cast<float>(reinterpret_cast<unsigned short*>(
                            buffer)[channels * i + c]));
                else if (type == BufferType::Int32)
                    upper[c] =
                        std::max(upper[c],
                                 static_cast<float>(reinterpret_cast<int*>(
                                     buffer)[channels * i + c]));
            }
        }
    }

    // For single channel buffers: fill with 0
    for (int c = channels; c < 4; ++c)
        upper[c] = 0.0;
}


void Buffer::reset_contrast_brightness_parameters()
{
    recompute_min_color_values();
    recompute_max_color_values();

    compute_contrast_brightness_parameters();
}


void Buffer::compute_contrast_brightness_parameters()
{
    float* lowest = min_buffer_values();
    float* upper  = max_buffer_values();

    float* auto_buffer_contrast   = auto_buffer_contrast_brightness_;
    float* auto_buffer_brightness = auto_buffer_contrast_brightness_ + 4;

    for (int c = 0; c < channels; ++c) {
        float maxIntensity = 1.0f;
        if (type == BufferType::UnsignedByte)
            maxIntensity = 255.0f;
        else if (type ==
                 BufferType::Short) // All non-real values have max color 255
            maxIntensity = std::numeric_limits<short>::max();
        else if (type == BufferType::UnsignedShort)
            maxIntensity = std::numeric_limits<unsigned short>::max();
        else if (type == BufferType::Int32)
            maxIntensity = std::numeric_limits<int>::max();
        else if (type == BufferType::Float32 || type == BufferType::Float64)
            maxIntensity = 1.0f;

        float upp_minus_low = upper[c] - lowest[c];

        if (upp_minus_low == 0)
            upp_minus_low = 1.0;

        auto_buffer_contrast[c] = maxIntensity / upp_minus_low;
        auto_buffer_brightness[c] =
            -lowest[c] / maxIntensity * auto_buffer_contrast[c];
    }
    for (int c = channels; c < 4; ++c) {
        auto_buffer_contrast[c]   = auto_buffer_contrast[0];
        auto_buffer_brightness[c] = auto_buffer_brightness[0];
    }
}


int Buffer::sub_texture_id_at_coord(int x, int y)
{
    int tx = x / max_texture_size;
    int ty = y / max_texture_size;
    return buff_tex[ty * num_textures_x + tx];
}


void Buffer::set_pixel_layout(const string& pixel_layout)
{
    ///
    // Make sure the provided pixel_layout is valid
    if (pixel_layout.size() != 4) {
        return;
    }
    const char valid_characters[] = {'r', 'g', 'b', 'a'};
    const int num_valid_chars =
        sizeof(valid_characters) / sizeof(valid_characters[0]);

    for (int i = 0; i < static_cast<int>(pixel_layout.size()); ++i) {
        bool is_character_valid = false;
        for (int j = 0; j < num_valid_chars; ++j) {
            if (pixel_layout[i] == valid_characters[j]) {
                is_character_valid = true;
                break;
            }
        }
        if (!is_character_valid) {
            return;
        }
    }

    ///
    // Copy the pixel format
    for (int i = 0; i < static_cast<int>(pixel_layout.size()); ++i) {
        pixel_layout_[i] = pixel_layout[i];
    }
}


const char* Buffer::get_pixel_layout() const
{
    return pixel_layout_;
}


float Buffer::tile_coord_x(int x)
{
    int buffer_width_i = static_cast<int>(buffer_width_f);
    int last_width     = buffer_width_i % max_texture_size;
    float tile_width =
        (x > (buffer_width_i - last_width)) ? last_width : max_texture_size;
    return static_cast<float>(x % max_texture_size) /
           static_cast<float>(tile_width - 1);
}


float Buffer::tile_coord_y(int y)
{
    int buffer_height_i = static_cast<int>(buffer_height_f);
    int last_height     = buffer_height_i % max_texture_size;
    int tile_height =
        (y > (buffer_height_i - last_height)) ? last_height : max_texture_size;
    return static_cast<float>(y % max_texture_size) /
           static_cast<float>(tile_height - 1);
}


void Buffer::update()
{
    GameObject* cam_obj = game_object->stage->get_game_object("camera");
    Camera* camera      = cam_obj->get_component<Camera>("camera_component");
    float zoom          = camera->get_zoom();

    buff_prog.use();
    if (zoom > 40) {
        buff_prog.uniform1i("enable_borders", 1);
    } else {
        buff_prog.uniform1i("enable_borders", 0);
    }
}


void Buffer::create_shader_program()
{
    // Buffer Shaders
    ShaderProgram::TexelChannels channel_type;
    if (channels == 1) {
        channel_type = ShaderProgram::FormatR;
    } else if (channels == 2) {
        channel_type = ShaderProgram::FormatRG;
    } else if (channels == 3) {
        channel_type = ShaderProgram::FormatRGB;
    } else {
        assert(channels == 4);
        channel_type = ShaderProgram::FormatRGBA;
    }

    buff_prog.create(shader::buff_vert_shader,
                     shader::buff_frag_shader,
                     channel_type,
                     pixel_layout_,
                     {"mvp",
                      "sampler",
                      "brightness_contrast",
                      "buffer_dimension",
                      "enable_borders"});
}


bool Buffer::initialize()
{
    create_shader_program();

    // Buffer VBO
    // clang-format off
    static const GLfloat g_vertex_buffer_data[] = {
        -0.5f, -0.5f,
        0.5f, -0.5f,
        0.5f,  0.5f,
        0.5f,  0.5f,
        -0.5f,  0.5f,
        -0.5f, -0.5f,
    };
    // clang-format on

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(g_vertex_buffer_data),
                 g_vertex_buffer_data,
                 GL_STATIC_DRAW);

    setup_gl_buffer();

    return true;
}


void Buffer::draw(const mat4& projection, const mat4& viewInv)
{
    buff_prog.use();
    mat4 model = game_object->get_pose();
    mat4 mvp   = projection * viewInv * model;

    glEnableVertexAttribArray(0);
    glActiveTexture(GL_TEXTURE0);
    buff_prog.uniform1i("sampler", 0);
    if (game_object->stage->contrast_enabled) {
        buff_prog.uniform4fv(
            "brightness_contrast", 2, auto_buffer_contrast_brightness_);
    } else {
        buff_prog.uniform4fv("brightness_contrast", 2, no_ac_params);
    }

    int buffer_width_i  = static_cast<int>(buffer_width_f);
    int buffer_height_i = static_cast<int>(buffer_height_f);

    int remaining_h = buffer_height_i;

    float py = -buffer_height_i / 2;
    if (buffer_height_i % 2 == 1) {
        py -= 0.5;
    }

    for (int ty = 0; ty < num_textures_y; ++ty) {
        int buff_h = std::min(remaining_h, max_texture_size);
        remaining_h -= buff_h;

        py += buff_h / 2;
        if (buff_h % 2 == 1) {
            py += 0.5;
        }

        int remaining_w = buffer_width_i;

        float px = -buffer_width_i / 2;
        if (buffer_width_i % 2 == 1) {
            px -= 0.5;
        }

        for (int tx = 0; tx < num_textures_x; ++tx) {
            int buff_w = std::min(remaining_w, max_texture_size);
            remaining_w -= buff_w;

            glBindTexture(GL_TEXTURE_2D, buff_tex[ty * num_textures_x + tx]);

            mat4 tile_model;

            px += buff_w / 2;
            if (buff_w % 2 == 1) {
                px += 0.5;
            }

            tile_model.set_from_st(buff_w, buff_h, 1.0, px, py, 0.0f);
            buff_prog.uniform_matrix4fv(
                "mvp", 1, GL_FALSE, (mvp * tile_model).data());
            buff_prog.uniform2f("buffer_dimension", buff_w, buff_h);

            px += buff_w / 2;

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        py += buff_h / 2;
    }
}


float* Buffer::min_buffer_values()
{
    return min_buffer_values_;
}


float* Buffer::max_buffer_values()
{
    return max_buffer_values_;
}


const float* Buffer::auto_buffer_contrast_brightness() const
{
    return auto_buffer_contrast_brightness_;
}


void Buffer::setup_gl_buffer()
{
    int buffer_width_i  = static_cast<int>(buffer_width_f);
    int buffer_height_i = static_cast<int>(buffer_height_f);

    game_object->scale    = {1.0, 1.0, 1.0f, 0.0f};
    game_object->position = {0.f, 0.f, 0.f, 1.f};

    // Initialize contrast parameters
    reset_contrast_brightness_parameters();

    // Buffer texture
    num_textures_x = ceil(((float)buffer_width_i) / ((float)max_texture_size));
    num_textures_y = ceil(((float)buffer_height_i) / ((float)max_texture_size));
    int num_textures = num_textures_x * num_textures_y;

    buff_tex.resize(num_textures);
    glGenTextures(num_textures, buff_tex.data());

    GLuint tex_type   = GL_UNSIGNED_BYTE;
    GLuint tex_format = GL_RED;

    if (type == BufferType::Float32 || type == BufferType::Float64) {
        tex_type = GL_FLOAT;
    } else if (type == BufferType::UnsignedByte) {
        tex_type = GL_UNSIGNED_BYTE;
    } else if (type == BufferType::Short) {
        tex_type = GL_SHORT;
    } else if (type == BufferType::UnsignedShort) {
        tex_type = GL_UNSIGNED_SHORT;
    } else if (type == BufferType::Int32) {
        tex_type = GL_INT;
    }

    if (channels == 1) {
        tex_format = GL_RED;
    } else if (channels == 2) {
        tex_format = GL_RG;
    } else if (channels == 3) {
        tex_format = GL_RGB;
    } else if (channels == 4) {
        tex_format = GL_RGBA;
    }

    int remaining_h = buffer_height_i;

    glPixelStoref(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, step);

    for (int ty = 0; ty < num_textures_y; ++ty) {
        int buff_h = std::min(remaining_h, max_texture_size);
        remaining_h -= buff_h;

        int remaining_w = buffer_width_i;
        for (int tx = 0; tx < num_textures_x; ++tx) {
            int buff_w = std::min(remaining_w, max_texture_size);
            remaining_w -= buff_w;

            int tex_id = ty * num_textures_x + tx;
            glBindTexture(GL_TEXTURE_2D, buff_tex[tex_id]);

            glPixelStorei(GL_UNPACK_SKIP_ROWS, ty * max_texture_size);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, tx * max_texture_size);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, buff_w, buff_h);

            glTexSubImage2D(GL_TEXTURE_2D,
                            0,
                            0,
                            0,
                            buff_w,
                            buff_h,
                            tex_format,
                            tex_type,
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
