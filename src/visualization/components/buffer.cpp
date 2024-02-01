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

#include "buffer.h"

#include <limits>

#include "GL/gl.h"

#include "camera.h"
#include "visualization/game_object.h"
#include "visualization/shaders/oid_shaders.h"
#include "visualization/stage.h"

using namespace std;


const float Buffer::no_ac_params[8] = {1.0, 1.0, 1.0, 1.0, 0, 0, 0, 0};


Buffer::Buffer(GameObject* game_object, GLCanvas* gl_canvas)
    : Component(game_object, gl_canvas)
    , type{BufferType::UnsignedByte}
    , buff_prog(gl_canvas)
{
}


Buffer::~Buffer()
{
    const int num_textures = num_textures_x * num_textures_y;

    gl_canvas_->glDeleteTextures(num_textures, buff_tex.data());
    gl_canvas_->glDeleteBuffers(1, &vbo);
}


bool Buffer::buffer_update()
{
    const int num_textures = num_textures_x * num_textures_y;
    glDeleteTextures(num_textures, buff_tex.data());

    create_shader_program();
    setup_gl_buffer();
    return true;
}


void Buffer::get_pixel_info(stringstream& message,
                            const int x,
                            const int y) const
{
    if (x < 0 || static_cast<float>(x) >= buffer_width_f || y < 0 ||
        static_cast<float>(y) >= buffer_height_f) {
        message << "[out of bounds]";
        return;
    }

    const int pos = channels * (y * step + x);

    message << "[";

    for (int c = 0; c < channels; ++c) {
        if (type == BufferType::Float32 || type == BufferType::Float64) {
            const float fpix = reinterpret_cast<const float*>(buffer)[pos + c];
            message << fpix;
        } else if (type == BufferType::UnsignedByte) {
            const short fpix = buffer[pos + c];
            message << fpix;
        } else if (type == BufferType::Short) {
            const short fpix = reinterpret_cast<const short*>(buffer)[pos + c];
            message << fpix;
        } else if (type == BufferType::UnsignedShort) {
            const unsigned short fpix =
                reinterpret_cast<const unsigned short*>(buffer)[pos + c];
            message << fpix;
        } else if (type == BufferType::Int32) {
            const int fpix = reinterpret_cast<const int*>(buffer)[pos + c];
            message << fpix;
        }
        if (c < channels - 1) {
            message << " ";
        }
    }
    message << "]";
}


void Buffer::rotate(const float angle)
{
    angle_ += angle;
}


void Buffer::set_icon_drawing_mode(bool is_enabled)
{
    buff_prog.use();

    buff_prog.uniform1i("enable_icon_mode", is_enabled ? 1 : 0);
}


void Buffer::recompute_min_color_values()
{
    const int buffer_width_i  = static_cast<int>(buffer_width_f);
    const int buffer_height_i = static_cast<int>(buffer_height_f);

    float* lowest = min_buffer_values();

    for (int i = 0; i < 4; ++i) {
        lowest[i] = (std::numeric_limits<float>::max)();
    }

    for (int y = 0; y < buffer_height_i; ++y) {
        for (int x = 0; x < buffer_width_i; ++x) {
            const int i = y * step + x;
            for (int c = 0; c < channels; ++c) {
                if (type == BufferType::Float32 ||
                    type == BufferType::Float64) {
                    lowest[c] = (std::min)(lowest[c],
                                           reinterpret_cast<const float*>(
                                               buffer)[channels * i + c]);
                } else if (type == BufferType::UnsignedByte) {
                    lowest[c] = (std::min)(
                        lowest[c],
                        static_cast<float>(buffer[channels * i + c]));
                } else if (type == BufferType::Short) {
                    lowest[c] = (std::min)(
                        lowest[c],
                        static_cast<float>(reinterpret_cast<const short*>(
                            buffer)[channels * i + c]));
                } else if (type == BufferType::UnsignedShort) {
                    lowest[c] =
                        (std::min)(lowest[c],
                                   static_cast<float>(
                                       reinterpret_cast<const unsigned short*>(
                                           buffer)[channels * i + c]));
                } else if (type == BufferType::Int32) {
                    lowest[c] = (std::min)(
                        lowest[c],
                        static_cast<float>(reinterpret_cast<const int*>(
                            buffer)[channels * i + c]));
                }
            }
        }
    }

    // For single channel buffers: fill with 0
    for (int c = channels; c < 4; ++c) {
        lowest[c] = 0.0;
    }
}


void Buffer::recompute_max_color_values()
{
    const int buffer_width_i  = static_cast<int>(buffer_width_f);
    const int buffer_height_i = static_cast<int>(buffer_height_f);

    float* upper = max_buffer_values();
    for (int i = 0; i < 4; ++i) {
        upper[i] = std::numeric_limits<float>::lowest();
    }

    for (int y = 0; y < buffer_height_i; ++y) {
        for (int x = 0; x < buffer_width_i; ++x) {
            const int i = y * step + x;
            for (int c = 0; c < channels; ++c) {
                if (type == BufferType::Float32 ||
                    type == BufferType::Float64) {
                    upper[c] = (std::max)(upper[c],
                                          reinterpret_cast<const float*>(
                                              buffer)[channels * i + c]);
                } else if (type == BufferType::UnsignedByte) {
                    upper[c] = (std::max)(
                        upper[c], static_cast<float>(buffer[channels * i + c]));
                } else if (type == BufferType::Short) {
                    upper[c] = (std::max)(
                        upper[c],
                        static_cast<float>(reinterpret_cast<const short*>(
                            buffer)[channels * i + c]));
                } else if (type == BufferType::UnsignedShort) {
                    upper[c] =
                        (std::max)(upper[c],
                                   static_cast<float>(
                                       reinterpret_cast<const unsigned short*>(
                                           buffer)[channels * i + c]));
                } else if (type == BufferType::Int32) {
                    upper[c] = (std::max)(
                        upper[c],
                        static_cast<float>(reinterpret_cast<const int*>(
                            buffer)[channels * i + c]));
                }
            }
        }
    }

    // For single channel buffers: fill with 0
    for (int c = channels; c < 4; ++c) {
        upper[c] = 0.0;
    }
}


void Buffer::reset_contrast_brightness_parameters()
{
    recompute_min_color_values();
    recompute_max_color_values();

    compute_contrast_brightness_parameters();
}


void Buffer::compute_contrast_brightness_parameters()
{
    const float* lowest = min_buffer_values();
    const float* upper  = max_buffer_values();

    float* auto_buffer_contrast   = auto_buffer_contrast_brightness_;
    float* auto_buffer_brightness = auto_buffer_contrast_brightness_ + 4;

    for (int c = 0; c < channels; ++c) {
        float maxIntensity = 1.0f;
        if (type == BufferType::UnsignedByte) {
            maxIntensity = 255.0f;
        } else if (type == BufferType::Short) {
            // All non-real values have max color 255
            maxIntensity =
                static_cast<float>((std::numeric_limits<short>::max)());
        } else if (type == BufferType::UnsignedShort) {
            maxIntensity = static_cast<float>(
                (std::numeric_limits<unsigned short>::max)());
        } else if (type == BufferType::Int32) {
            maxIntensity =
                static_cast<float>((std::numeric_limits<int>::max)());
        } else if (type == BufferType::Float32 || type == BufferType::Float64) {
            maxIntensity = 1.0f;
        }
        float upp_minus_low = upper[c] - lowest[c];

        if (upp_minus_low == 0.0f) {
            upp_minus_low = 1.0f;
        }

        auto_buffer_contrast[c] = maxIntensity / upp_minus_low;
        auto_buffer_brightness[c] =
            -lowest[c] / maxIntensity * auto_buffer_contrast[c];
    }
    for (int c = channels; c < 4; ++c) {
        auto_buffer_contrast[c]   = auto_buffer_contrast[0];
        auto_buffer_brightness[c] = auto_buffer_brightness[0];
    }
}


int Buffer::sub_texture_id_at_coord(const int x, const int y) const
{
    const int tx = x / max_texture_size;
    const int ty = y / max_texture_size;
    return static_cast<int>(buff_tex[ty * num_textures_x + tx]);
}


void Buffer::set_pixel_layout(const string& pixel_layout)
{
    ///
    // Make sure the provided pixel_layout is valid
    if (pixel_layout.size() != 4) {
        return;
    }
    constexpr char valid_characters[] = {'r', 'g', 'b', 'a'};

    for (const char i : pixel_layout) {
        bool is_character_valid = false;
        for (const char valid_character : valid_characters) {
            if (i == valid_character) {
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


float Buffer::tile_coord_x(const int x) const
{
    const int buffer_width_i = static_cast<int>(buffer_width_f);
    const int last_width     = buffer_width_i % max_texture_size;
    const int tile_width =
        x > buffer_width_i - last_width ? last_width : max_texture_size;
    return static_cast<float>(x % max_texture_size) /
           static_cast<float>(tile_width - 1);
}


float Buffer::tile_coord_y(int y) const
{
    const int buffer_height_i = static_cast<int>(buffer_height_f);
    const int last_height     = buffer_height_i % max_texture_size;
    const int tile_height =
        y > buffer_height_i - last_height ? last_height : max_texture_size;
    return static_cast<float>(y % max_texture_size) /
           static_cast<float>(tile_height - 1);
}


void Buffer::update()
{
    GameObject* cam_obj = game_object_->stage->get_game_object("camera");
    const auto* camera  = cam_obj->get_component<Camera>("camera_component");
    const float zoom    = camera->compute_zoom();

    buff_prog.use();
    if (zoom > 40.0f) {
        buff_prog.uniform1i("enable_borders", 1);
    } else {
        buff_prog.uniform1i("enable_borders", 0);
    }

    update_object_pose();
}


void Buffer::update_object_pose() const
{
    const mat4 rotation = mat4::rotation(angle_);

    mat4 transposition;

    if (transpose) {
        // clang-format off
        transposition << std::initializer_list<float>{
            0, 1, 0, 0,
            1, 0, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        // clang-format on
    } else {
        transposition.set_identity();
    }

    game_object_->set_pose(rotation * transposition);
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
                      "enable_borders",
                      "enable_icon_mode"});
}


bool Buffer::initialize()
{
    create_shader_program();

    // Buffer VBO
    // clang-format off
    static constexpr GLfloat g_vertex_buffer_data[] = {
        -0.5f, -0.5f,
        0.5f, -0.5f,
        0.5f, 0.5f,
        0.5f, 0.5f,
        -0.5f, 0.5f,
        -0.5f, -0.5f,
    };
    // clang-format on

    gl_canvas_->glGenBuffers(1, &vbo);
    gl_canvas_->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gl_canvas_->glBufferData(GL_ARRAY_BUFFER,
                             sizeof(g_vertex_buffer_data),
                             g_vertex_buffer_data,
                             GL_STATIC_DRAW);

    setup_gl_buffer();

    update_object_pose();

    return true;
}


void Buffer::draw(const mat4& projection, const mat4& viewInv)
{
    buff_prog.use();
    const mat4 model = game_object_->get_pose();
    const mat4 mvp   = projection * viewInv * model;

    gl_canvas_->glEnableVertexAttribArray(0);
    gl_canvas_->glActiveTexture(GL_TEXTURE0);

    buff_prog.uniform1i("sampler", 0);
    if (game_object_->stage->contrast_enabled) {
        buff_prog.uniform4fv(
            "brightness_contrast", 2, auto_buffer_contrast_brightness_);
    } else {
        buff_prog.uniform4fv("brightness_contrast", 2, no_ac_params);
    }

    const auto buffer_width_i  = static_cast<int>(buffer_width_f);
    const auto buffer_height_i = static_cast<int>(buffer_height_f);

    int remaining_h = buffer_height_i;

    float py = static_cast<float>(-buffer_height_i) / 2.0f;
    if (buffer_height_i % 2 == 1) {
        py -= 0.5f;
    }

    for (int ty = 0; ty < num_textures_y; ++ty) {
        const int buff_h = (std::min)(remaining_h, max_texture_size);
        remaining_h -= buff_h;

        py += static_cast<float>(buff_h) / 2.0f;
        if (buff_h % 2 == 1) {
            py += 0.5f;
        }

        int remaining_w = buffer_width_i;

        float px = static_cast<float>(-buffer_width_i) / 2.0f;
        if (buffer_width_i % 2 == 1) {
            px -= 0.5f;
        }

        for (int tx = 0; tx < num_textures_x; ++tx) {
            const int buff_w = (std::min)(remaining_w, max_texture_size);
            remaining_w -= buff_w;

            glBindTexture(GL_TEXTURE_2D, buff_tex[ty * num_textures_x + tx]);

            mat4 tile_model;

            px += static_cast<float>(buff_w) / 2.0f;
            if (buff_w % 2 == 1) {
                px += 0.5f;
            }

            tile_model.set_from_st(static_cast<float>(buff_w),
                                   static_cast<float>(buff_h),
                                   1.0f,
                                   px,
                                   py,
                                   0.0f);
            buff_prog.uniform_matrix4fv(
                "mvp", 1, GL_FALSE, (mvp * tile_model).data());
            buff_prog.uniform2f("buffer_dimension",
                                static_cast<float>(buff_w),
                                static_cast<float>(buff_h));

            px += static_cast<float>(buff_w) / 2;

            gl_canvas_->glBindBuffer(GL_ARRAY_BUFFER, vbo);
            gl_canvas_->glVertexAttribPointer(
                0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            gl_canvas_->glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        py += static_cast<float>(buff_h) / 2.0f;
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
    const int buffer_width_i  = static_cast<int>(buffer_width_f);
    const int buffer_height_i = static_cast<int>(buffer_height_f);

    // Initialize contrast parameters
    reset_contrast_brightness_parameters();

    // Buffer texture
    num_textures_x         = ceil(static_cast<float>(buffer_width_i) /
                          static_cast<float>(max_texture_size));
    num_textures_y         = ceil(static_cast<float>(buffer_height_i) /
                          static_cast<float>(max_texture_size));
    const int num_textures = num_textures_x * num_textures_y;

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
        const int buff_h = (std::min)(remaining_h, max_texture_size);
        remaining_h -= buff_h;

        int remaining_w = buffer_width_i;
        for (int tx = 0; tx < num_textures_x; ++tx) {
            const int buff_w = (std::min)(remaining_w, max_texture_size);
            remaining_w -= buff_w;

            const int tex_id = ty * num_textures_x + tx;
            gl_canvas_->glBindTexture(GL_TEXTURE_2D, buff_tex[tex_id]);

            gl_canvas_->glPixelStorei(GL_UNPACK_SKIP_ROWS,
                                      ty * max_texture_size);
            gl_canvas_->glPixelStorei(GL_UNPACK_SKIP_PIXELS,
                                      tx * max_texture_size);

            gl_canvas_->glTexImage2D(GL_TEXTURE_2D,
                                     0,
                                     GL_RGBA32F,
                                     buff_w,
                                     buff_h,
                                     0,
                                     tex_format,
                                     tex_type,
                                     nullptr);

            gl_canvas_->glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                buff_w,
                buff_h,
                tex_format,
                tex_type,
                reinterpret_cast<const GLvoid*>(buffer));

            gl_canvas_->glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            gl_canvas_->glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            gl_canvas_->glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl_canvas_->glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            gl_canvas_->glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }
    }

    gl_canvas_->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl_canvas_->glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    gl_canvas_->glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
}
