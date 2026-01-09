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

#include "buffer.h"

#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <string>

#include "GL/gl.h"

#include "camera.h"
#include "math/linear_algebra.h"
#include "visualization/game_object.h"
#include "visualization/shaders/oid_shaders.h"
#include "visualization/stage.h"

namespace oid
{

namespace
{

// Helper function to validate buffer dimension
bool validate_dimension(const int dimension,
                        const char* dimension_name,
                        const int min_value,
                        const int max_value)
{
    if (dimension < min_value || dimension > max_value) {
        std::cerr << "[Error] Invalid buffer " << dimension_name << ": "
                  << dimension << " (must be between " << min_value << " and "
                  << max_value << ")" << std::endl;
        return false;
    }
    return true;
}

} // namespace

constexpr std::array<float, 8>
    Buffer::no_ac_params{1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};


Buffer::Buffer(GameObject& game_object, GLCanvas& gl_canvas)
    : Component{game_object, gl_canvas}
    , buff_prog_{gl_canvas}
{
}


Buffer::~Buffer() noexcept
{
    const auto num_textures = num_textures_x * num_textures_y;

    gl_canvas_.glDeleteTextures(num_textures, buff_tex.data());
    gl_canvas_.glDeleteBuffers(1, &vbo_);
}


bool Buffer::buffer_update()
{
    const auto num_textures = num_textures_x * num_textures_y;
    gl_canvas_.glDeleteTextures(num_textures, buff_tex.data());

    if (!create_shader_program()) {
        return false;
    }
    setup_gl_buffer();
    return true;
}


void Buffer::get_pixel_info(std::stringstream& message,
                            const int x,
                            const int y) const
{
    if (x < 0 || static_cast<float>(x) >= buffer_width_f || y < 0 ||
        static_cast<float>(y) >= buffer_height_f) {
        message << "[out of bounds]";
        return;
    }

    const auto pos = channels * (y * step + x);

    message << "[";

    for (int c = 0; c < channels; ++c) {
        if (type == BufferType::Float32 || type == BufferType::Float64) {
            const auto fpix =
                std::bit_cast<const float*>(buffer_.data())[pos + c];
            message << fpix;
        } else if (type == BufferType::UnsignedByte) {
            const auto fpix =
                static_cast<short>(static_cast<uint8_t>(buffer_[pos + c]));
            message << fpix;
        } else if (type == BufferType::Short) {
            const auto fpix =
                std::bit_cast<const short*>(buffer_.data())[pos + c];
            message << fpix;
        } else if (type == BufferType::UnsignedShort) {
            const auto fpix =
                std::bit_cast<const unsigned short*>(buffer_.data())[pos + c];
            message << fpix;
        } else if (type == BufferType::Int32) {
            const auto fpix =
                std::bit_cast<const int*>(buffer_.data())[pos + c];
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


void Buffer::set_icon_drawing_mode(const bool is_enabled) const
{
    buff_prog_.use();

    buff_prog_.uniform1i("enable_icon_mode", is_enabled ? 1 : 0);
}


void Buffer::update_min_color_value(float* lowest,
                                    const int i,
                                    const int c) const
{
    if (type == BufferType::Float32 || type == BufferType::Float64) {
        lowest[c] = (std::min)(lowest[c],
                               std::bit_cast<const float*>(
                                   buffer_.data())[channels * i + c]);
    } else if (type == BufferType::UnsignedByte) {
        lowest[c] = (std::min)(lowest[c],
                               static_cast<float>(static_cast<uint8_t>(
                                   buffer_[channels * i + c])));
    } else if (type == BufferType::Short) {
        lowest[c] = (std::min)(lowest[c],
                               static_cast<float>(std::bit_cast<const short*>(
                                   buffer_.data())[channels * i + c]));
    } else if (type == BufferType::UnsignedShort) {
        lowest[c] =
            (std::min)(lowest[c],
                       static_cast<float>(std::bit_cast<const unsigned short*>(
                           buffer_.data())[channels * i + c]));
    } else if (type == BufferType::Int32) {
        lowest[c] = (std::min)(lowest[c],
                               static_cast<float>(std::bit_cast<const int*>(
                                   buffer_.data())[channels * i + c]));
    }
}


void Buffer::recompute_min_color_values()
{
    const auto buffer_width_i  = static_cast<int>(buffer_width_f);
    const auto buffer_height_i = static_cast<int>(buffer_height_f);

    auto lowest = min_buffer_values();

    for (int i = 0; i < 4; ++i) {
        lowest[i] = (std::numeric_limits<float>::max)();
    }

    for (int y = 0; y < buffer_height_i; ++y) {
        for (int x = 0; x < buffer_width_i; ++x) {
            const auto i = y * step + x;
            for (int c = 0; c < channels; ++c) {
                update_min_color_value(lowest.data(), i, c);
            }
        }
    }

    // For single channel buffers: fill with 0
    for (int c = channels; c < 4; ++c) {
        lowest[c] = 0.0;
    }
}


void Buffer::update_max_color_value(float* upper,
                                    const int i,
                                    const int c) const
{
    if (type == BufferType::Float32 || type == BufferType::Float64) {
        upper[c] = (std::max)(upper[c],
                              std::bit_cast<const float*>(
                                  buffer_.data())[channels * i + c]);
    } else if (type == BufferType::UnsignedByte) {
        upper[c] = (std::max)(upper[c],
                              static_cast<float>(static_cast<uint8_t>(
                                  buffer_[channels * i + c])));
    } else if (type == BufferType::Short) {
        upper[c] = (std::max)(upper[c],
                              static_cast<float>(std::bit_cast<const short*>(
                                  buffer_.data())[channels * i + c]));
    } else if (type == BufferType::UnsignedShort) {
        upper[c] =
            (std::max)(upper[c],
                       static_cast<float>(std::bit_cast<const unsigned short*>(
                           buffer_.data())[channels * i + c]));
    } else if (type == BufferType::Int32) {
        upper[c] = (std::max)(upper[c],
                              static_cast<float>(std::bit_cast<const int*>(
                                  buffer_.data())[channels * i + c]));
    }
}


void Buffer::recompute_max_color_values()
{
    const auto buffer_width_i  = static_cast<int>(buffer_width_f);
    const auto buffer_height_i = static_cast<int>(buffer_height_f);

    auto upper = max_buffer_values();
    for (int i = 0; i < 4; ++i) {
        upper[i] = std::numeric_limits<float>::lowest();
    }

    for (int y = 0; y < buffer_height_i; ++y) {
        for (int x = 0; x < buffer_width_i; ++x) {
            const auto i = y * step + x;
            for (int c = 0; c < channels; ++c) {
                update_max_color_value(upper.data(), i, c);
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
    const auto lowest = min_buffer_values();
    const auto upper  = max_buffer_values();

    const auto auto_buffer_contrast = auto_buffer_contrast_brightness_.data();
    const auto auto_buffer_brightness =
        auto_buffer_contrast_brightness_.data() + 4;

    for (int c = 0; c < channels; ++c) {
        auto maxIntensity = 1.0f;
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
        auto upp_minus_low = upper[c] - lowest[c];

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
    const auto tx = x / max_texture_size;
    const auto ty = y / max_texture_size;
    return static_cast<int>(buff_tex[ty * num_textures_x + tx]);
}


void Buffer::configure(const BufferParams& params)
{
    // Validate buffer span
    if (params.buffer.data() == nullptr || params.buffer.empty()) {
        std::cerr << "[Error] Buffer span is null or empty" << std::endl;
        return;
    }

    // Validate dimensions
    if (!validate_dimension(params.buffer_width_i,
                            "width",
                            BufferConstants::MIN_BUFFER_DIMENSION,
                            BufferConstants::MAX_BUFFER_DIMENSION)) {
        return;
    }

    if (!validate_dimension(params.buffer_height_i,
                            "height",
                            BufferConstants::MIN_BUFFER_DIMENSION,
                            BufferConstants::MAX_BUFFER_DIMENSION)) {
        return;
    }

    // Validate channel count
    if (params.channels < BufferConstants::MIN_CHANNELS ||
        params.channels > BufferConstants::MAX_CHANNELS) {
        std::cerr << "[Error] Invalid channel count: " << params.channels
                  << " (must be between " << BufferConstants::MIN_CHANNELS
                  << " and " << BufferConstants::MAX_CHANNELS << ")"
                  << std::endl;
        return;
    }

    // Validate step (must be positive and at least as large as channels)
    if (params.step < params.channels) {
        std::cerr << "[Error] Invalid step: " << params.step
                  << " (must be >= channels: " << params.channels << ")"
                  << std::endl;
        return;
    }

    // Validate buffer size (prevent potential DoS)
    // Calculate: width * height * step (step is stride in bytes per row)
    // Use checked multiplication to prevent overflow
    const auto width_u  = static_cast<std::size_t>(params.buffer_width_i);
    const auto height_u = static_cast<std::size_t>(params.buffer_height_i);
    const auto step_u   = static_cast<std::size_t>(params.step);

    // Check for potential overflow before multiplication
    if (width_u > 0 && height_u > BufferConstants::MAX_BUFFER_SIZE / width_u) {
        std::cerr << "[Error] Buffer dimensions too large (would overflow)"
                  << std::endl;
        return;
    }

    if (const auto buffer_size = width_u * height_u * step_u;
        buffer_size > BufferConstants::MAX_BUFFER_SIZE) {
        std::cerr << "[Error] Buffer size too large: " << buffer_size
                  << " bytes (maximum: " << BufferConstants::MAX_BUFFER_SIZE
                  << " bytes)" << std::endl;
        return;
    }

    buffer_         = params.buffer;
    channels        = params.channels;
    type            = params.type;
    buffer_width_f  = static_cast<float>(params.buffer_width_i);
    buffer_height_f = static_cast<float>(params.buffer_height_i);
    step            = params.step;
    transpose       = params.transpose_buffer;
    set_pixel_layout(params.pixel_layout);
}


void Buffer::set_pixel_layout(const std::string& pixel_layout)
{
    ///
    // Make sure the provided pixel_layout is valid
    if (pixel_layout.size() != 4) {
        return;
    }

    for (const auto i : pixel_layout) {
        auto is_character_valid = false;
        for (const auto valid_character : {'r', 'g', 'b', 'a'}) {
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
    return pixel_layout_.data();
}


float Buffer::tile_coord_x(const int x) const
{
    const auto buffer_width_i = static_cast<int>(buffer_width_f);
    const auto last_width     = buffer_width_i % max_texture_size;
    const auto tile_width =
        x > buffer_width_i - last_width ? last_width : max_texture_size;
    return static_cast<float>(x % max_texture_size) /
           static_cast<float>(tile_width - 1);
}


float Buffer::tile_coord_y(int y) const
{
    const auto buffer_height_i = static_cast<int>(buffer_height_f);
    const auto last_height     = buffer_height_i % max_texture_size;
    const auto tile_height =
        y > buffer_height_i - last_height ? last_height : max_texture_size;
    return static_cast<float>(y % max_texture_size) /
           static_cast<float>(tile_height - 1);
}


void Buffer::update()
{
    const auto stage = game_object_.get_stage();
    if (!stage.has_value()) {
        return;
    }
    const auto cam_obj = stage->get().get_game_object("camera");
    if (!cam_obj.has_value()) {
        return;
    }
    const auto camera_opt =
        cam_obj->get().get_component<Camera>("camera_component");
    if (!camera_opt.has_value()) {
        return;
    }
    const auto& camera = camera_opt->get();
    const auto zoom    = camera.compute_zoom();

    buff_prog_.use();
    if (zoom > BufferConstants::ZOOM_BORDER_THRESHOLD) {
        buff_prog_.uniform1i("enable_borders", 1);
    } else {
        buff_prog_.uniform1i("enable_borders", 0);
    }

    update_object_pose();
}


void Buffer::update_object_pose() const
{
    const auto rotation = mat4::rotation(angle_);

    auto transposition = mat4{};

    if (transpose) {
        // clang-format off
        transposition << std::initializer_list<float>{
            0.0f, 1.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        // clang-format on
    } else {
        transposition.set_identity();
    }

    game_object_.set_pose(rotation * transposition);
}


bool Buffer::create_shader_program()
{
    // Buffer Shaders
    auto channel_type = ShaderProgram::TexelChannels{};
    if (channels == 1) {
        channel_type = ShaderProgram::TexelChannels::FormatR;
    } else if (channels == 2) {
        channel_type = ShaderProgram::TexelChannels::FormatRG;
    } else if (channels == 3) {
        channel_type = ShaderProgram::TexelChannels::FormatRGB;
    } else {
        assert(channels == 4);
        channel_type = ShaderProgram::TexelChannels::FormatRGBA;
    }

    return buff_prog_.create(shader::buff_vert_shader,
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
    if (!create_shader_program()) {
        return false;
    }

    // Buffer VBO
    // clang-format off
    static constexpr auto g_vertex_buffer_data = std::array{
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f,
        -0.5f, -0.5f,
    };
    // clang-format on

    gl_canvas_.glGenBuffers(1, &vbo_);
    gl_canvas_.glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl_canvas_.glBufferData(GL_ARRAY_BUFFER,
                            sizeof(g_vertex_buffer_data),
                            g_vertex_buffer_data.data(),
                            GL_STATIC_DRAW);

    setup_gl_buffer();

    update_object_pose();

    return true;
}


void Buffer::draw(const mat4& projection, const mat4& viewInv)
{
    buff_prog_.use();
    const auto model = game_object_.get_pose();
    const auto mvp   = projection * viewInv * model;

    gl_canvas_.glEnableVertexAttribArray(0);
    gl_canvas_.glActiveTexture(GL_TEXTURE0);

    buff_prog_.uniform1i("sampler", 0);
    if (const auto stage = game_object_.get_stage();
        stage.has_value() && stage->get().get_contrast_enabled()) {
        buff_prog_.uniform4fv(
            "brightness_contrast", 2, auto_buffer_contrast_brightness_.data());
    } else {
        buff_prog_.uniform4fv("brightness_contrast", 2, no_ac_params.data());
    }

    const auto buffer_width_i  = static_cast<int>(buffer_width_f);
    const auto buffer_height_i = static_cast<int>(buffer_height_f);

    auto remaining_h = buffer_height_i;

    auto py = static_cast<float>(-buffer_height_i) / 2.0f;
    if (buffer_height_i % 2 == 1) {
        py -= 0.5f;
    }

    for (int ty = 0; ty < num_textures_y; ++ty) {
        const auto buff_h = (std::min)(remaining_h, max_texture_size);
        remaining_h -= buff_h;

        py += static_cast<float>(buff_h) / 2.0f;
        if (buff_h % 2 == 1) {
            py += 0.5f;
        }

        auto remaining_w = buffer_width_i;

        auto px = static_cast<float>(-buffer_width_i) / 2.0f;
        if (buffer_width_i % 2 == 1) {
            px -= 0.5f;
        }

        for (int tx = 0; tx < num_textures_x; ++tx) {
            const auto buff_w = (std::min)(remaining_w, max_texture_size);
            remaining_w -= buff_w;

            gl_canvas_.glBindTexture(GL_TEXTURE_2D,
                                     buff_tex[ty * num_textures_x + tx]);

            auto tile_model = mat4{};

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
            buff_prog_.uniform_matrix4fv(
                "mvp", 1, GL_FALSE, (mvp * tile_model).data());
            buff_prog_.uniform2f("buffer_dimension",
                                 static_cast<float>(buff_w),
                                 static_cast<float>(buff_h));

            px += static_cast<float>(buff_w) / 2.0f;

            gl_canvas_.glBindBuffer(GL_ARRAY_BUFFER, vbo_);
            gl_canvas_.glVertexAttribPointer(
                0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            gl_canvas_.glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        py += static_cast<float>(buff_h) / 2.0f;
    }
}


std::span<float> Buffer::min_buffer_values()
{
    return min_buffer_values_;
}


std::span<float> Buffer::max_buffer_values()
{
    return max_buffer_values_;
}

std::span<const float> Buffer::max_buffer_values() const
{
    return max_buffer_values_;
}


const float* Buffer::auto_buffer_contrast_brightness() const
{
    return auto_buffer_contrast_brightness_.data();
}


void Buffer::setup_gl_buffer()
{
    const auto buffer_width_i  = static_cast<int>(buffer_width_f);
    const auto buffer_height_i = static_cast<int>(buffer_height_f);

    // Initialize contrast parameters
    reset_contrast_brightness_parameters();

    // Buffer texture
    const auto max_texture_size_f = static_cast<float>(max_texture_size);
    num_textures_x                = static_cast<int>(
        std::ceil(static_cast<float>(buffer_width_i) / max_texture_size_f));
    num_textures_y = static_cast<int>(
        std::ceil(static_cast<float>(buffer_height_i) / max_texture_size_f));
    const int num_textures = num_textures_x * num_textures_y;

    buff_tex.resize(num_textures);
    gl_canvas_.glGenTextures(num_textures, buff_tex.data());

    auto tex_type   = GLuint{GL_UNSIGNED_BYTE};
    auto tex_format = GLuint{GL_RED};

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

    auto remaining_h = buffer_height_i;

    gl_canvas_.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl_canvas_.glPixelStorei(GL_UNPACK_ROW_LENGTH, step);

    for (int ty = 0; ty < num_textures_y; ++ty) {
        const auto buff_h = (std::min)(remaining_h, max_texture_size);
        remaining_h -= buff_h;

        auto remaining_w = buffer_width_i;
        for (int tx = 0; tx < num_textures_x; ++tx) {
            const auto buff_w = (std::min)(remaining_w, max_texture_size);
            remaining_w -= buff_w;

            const auto tex_id = ty * num_textures_x + tx;
            gl_canvas_.glBindTexture(GL_TEXTURE_2D, buff_tex[tex_id]);

            gl_canvas_.glPixelStorei(GL_UNPACK_SKIP_ROWS,
                                     ty * max_texture_size);
            gl_canvas_.glPixelStorei(GL_UNPACK_SKIP_PIXELS,
                                     tx * max_texture_size);

            gl_canvas_.glTexImage2D(GL_TEXTURE_2D,
                                    0,
                                    GL_RGBA32F,
                                    buff_w,
                                    buff_h,
                                    0,
                                    tex_format,
                                    tex_type,
                                    nullptr);

            gl_canvas_.glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                buff_w,
                buff_h,
                tex_format,
                tex_type,
                std::bit_cast<const GLvoid*>(buffer_.data()));

            gl_canvas_.glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            gl_canvas_.glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            gl_canvas_.glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl_canvas_.glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            gl_canvas_.glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }
    }

    gl_canvas_.glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl_canvas_.glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    gl_canvas_.glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
}

} // namespace oid
