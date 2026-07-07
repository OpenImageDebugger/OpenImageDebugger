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

#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <string>

#include "GL/gl.h"

#include "camera.h"
#include "math/linear_algebra.h"
#include "platform/gl_dialect.h"
#include "visualization/game_object.h"
#include "visualization/shaders/oid_shaders.h"
#include "visualization/stage.h"

namespace oid {

namespace {

// Helper function to validate buffer dimension
bool validate_dimension(const int dimension,
                        const char* dimension_name,
                        const int min_value,
                        const int max_value) {
    if (dimension < min_value || dimension > max_value) {
        std::cerr << "[Error] Invalid buffer " << dimension_name << ": "
                  << dimension << " (must be between " << min_value << " and "
                  << max_value << ")" << std::endl;
        return false;
    }
    return true;
}

// Helper function to format pixel value based on buffer type
void format_pixel_value(std::stringstream& message,
                        const BufferType type,
                        const std::span<const std::byte> buffer,
                        const int pos) {
    using enum BufferType;
    switch (type) {
    case FLOAT32:
        message << std::bit_cast<const float*>(buffer.data())[pos];
        break;
    case FLOAT64:
        message << std::bit_cast<const double*>(buffer.data())[pos];
        break;
    case UNSIGNED_BYTE:
        message << static_cast<short>(static_cast<uint8_t>(buffer[pos]));
        break;
    case SHORT:
        message << std::bit_cast<const short*>(buffer.data())[pos];
        break;
    case UNSIGNED_SHORT:
        message << std::bit_cast<const unsigned short*>(buffer.data())[pos];
        break;
    case INT32:
        message << std::bit_cast<const int*>(buffer.data())[pos];
        break;
    }
}

// Helper function to get channel range based on display mode
std::pair<int, int> get_channel_range(const int display_channel_mode,
                                      const int channels,
                                      const char* pixel_layout) {
    if (display_channel_mode == 1) {
        int selected_ch = 0;
        if (pixel_layout[0] == 'g') {
            selected_ch = 1;
        } else if (pixel_layout[0] == 'b') {
            selected_ch = 2;
        }
        return {selected_ch, selected_ch + 1};
    }
    return {0, channels};
}

} // namespace

constexpr std::array<float, 8> Buffer::no_ac_params{
    1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

Buffer::Buffer(const std::shared_ptr<GameObject>& game_object,
               const std::shared_ptr<RenderCanvas>& gl_canvas)
    : Component{game_object, gl_canvas}, buff_prog_{*gl_canvas} {}

Buffer::~Buffer() noexcept {
    if (const auto canvas = gl_canvas()) {
        const auto num_textures = num_textures_x_ * num_textures_y_;
        canvas->glDeleteTextures(num_textures, buff_tex_.data());
        canvas->glDeleteBuffers(1, &vbo_);
    }
}

bool Buffer::buffer_update() {
    const auto num_textures = num_textures_x_ * num_textures_y_;
    gl_canvas_ref().glDeleteTextures(num_textures, buff_tex_.data());

    if (!create_shader_program()) {
        return false;
    }
    setup_gl_buffer();
    return true;
}

void Buffer::get_pixel_info(std::stringstream& message,
                            const int x,
                            const int y) const {
    if (x < 0 || static_cast<float>(x) >= buffer_width_f_ || y < 0 ||
        static_cast<float>(y) >= buffer_height_f_) {
        message << "[out of bounds]";
        return;
    }

    const auto pos = channels_ * (y * step_ + x);
    const auto [start_ch, end_ch] = get_channel_range(
        display_channel_mode_, channels_, pixel_layout_.data());

    message << "[";
    for (int c = start_ch; c < end_ch && c < channels_; ++c) {
        if (c > start_ch) {
            message << " ";
        }
        format_pixel_value(message, type_, buffer_, pos + c);
    }
    message << "]";
}

void Buffer::rotate(const float angle) {
    angle_ += angle;
}

void Buffer::set_icon_drawing_mode(const bool is_enabled) const {
    buff_prog_.use();

    buff_prog_.uniform1i("enable_icon_mode", is_enabled ? 1 : 0);
}

void Buffer::update_min_color_value(float* lowest,
                                    const int i,
                                    const int c) const {
    using enum BufferType;
    if (type_ == FLOAT32 || type_ == FLOAT64) {
        lowest[c] = (std::min)(lowest[c],
                               std::bit_cast<const float*>(
                                   buffer_.data())[channels_ * i + c]);
    } else if (type_ == UNSIGNED_BYTE) {
        lowest[c] = (std::min)(lowest[c],
                               static_cast<float>(static_cast<uint8_t>(
                                   buffer_[channels_ * i + c])));
    } else if (type_ == SHORT) {
        lowest[c] = (std::min)(lowest[c],
                               static_cast<float>(std::bit_cast<const short*>(
                                   buffer_.data())[channels_ * i + c]));
    } else if (type_ == UNSIGNED_SHORT) {
        lowest[c] =
            (std::min)(lowest[c],
                       static_cast<float>(std::bit_cast<const unsigned short*>(
                           buffer_.data())[channels_ * i + c]));
    } else if (type_ == INT32) {
        lowest[c] = (std::min)(lowest[c],
                               static_cast<float>(std::bit_cast<const int*>(
                                   buffer_.data())[channels_ * i + c]));
    }
}

void Buffer::recompute_min_color_values() {
    const auto buffer_width_i = static_cast<int>(buffer_width_f_);
    const auto buffer_height_i = static_cast<int>(buffer_height_f_);

    auto lowest = min_buffer_values();

    for (int i = 0; i < 4; ++i) {
        lowest[i] = (std::numeric_limits<float>::max)();
    }

    for (int y = 0; y < buffer_height_i; ++y) {
        for (int x = 0; x < buffer_width_i; ++x) {
            const auto i = y * step_ + x;
            for (int c = 0; c < channels_; ++c) {
                update_min_color_value(lowest.data(), i, c);
            }
        }
    }

    // For single channel buffers: fill with 0
    for (int c = channels_; c < 4; ++c) {
        lowest[c] = 0.0;
    }
}

void Buffer::update_max_color_value(float* upper,
                                    const int i,
                                    const int c) const {
    using enum BufferType;
    if (type_ == FLOAT32 || type_ == FLOAT64) {
        upper[c] = (std::max)(upper[c],
                              std::bit_cast<const float*>(
                                  buffer_.data())[channels_ * i + c]);
    } else if (type_ == UNSIGNED_BYTE) {
        upper[c] = (std::max)(upper[c],
                              static_cast<float>(static_cast<uint8_t>(
                                  buffer_[channels_ * i + c])));
    } else if (type_ == SHORT) {
        upper[c] = (std::max)(upper[c],
                              static_cast<float>(std::bit_cast<const short*>(
                                  buffer_.data())[channels_ * i + c]));
    } else if (type_ == UNSIGNED_SHORT) {
        upper[c] =
            (std::max)(upper[c],
                       static_cast<float>(std::bit_cast<const unsigned short*>(
                           buffer_.data())[channels_ * i + c]));
    } else if (type_ == INT32) {
        upper[c] = (std::max)(upper[c],
                              static_cast<float>(std::bit_cast<const int*>(
                                  buffer_.data())[channels_ * i + c]));
    }
}

void Buffer::recompute_max_color_values() {
    const auto buffer_width_i = static_cast<int>(buffer_width_f_);
    const auto buffer_height_i = static_cast<int>(buffer_height_f_);

    auto upper = max_buffer_values();
    for (int i = 0; i < 4; ++i) {
        upper[i] = std::numeric_limits<float>::lowest();
    }

    for (int y = 0; y < buffer_height_i; ++y) {
        for (int x = 0; x < buffer_width_i; ++x) {
            const auto i = y * step_ + x;
            for (int c = 0; c < channels_; ++c) {
                update_max_color_value(upper.data(), i, c);
            }
        }
    }

    // For single channel buffers: fill with 0
    for (int c = channels_; c < 4; ++c) {
        upper[c] = 0.0;
    }
}

void Buffer::reset_contrast_brightness_parameters() {
    recompute_min_color_values();
    recompute_max_color_values();

    compute_contrast_brightness_parameters();
}

void Buffer::compute_contrast_brightness_parameters() {
    using enum BufferType;
    const auto lowest = min_buffer_values();
    const auto upper = max_buffer_values();

    const auto auto_buffer_contrast = auto_buffer_contrast_brightness_.data();
    const auto auto_buffer_brightness =
        auto_buffer_contrast_brightness_.data() + 4;

    for (int c = 0; c < channels_; ++c) {
        auto maxIntensity = 1.0f;
        if (type_ == UNSIGNED_BYTE) {
            maxIntensity = 255.0f;
        } else if (type_ == SHORT) {
            // All non-real values have max color 255
            maxIntensity =
                static_cast<float>((std::numeric_limits<short>::max)());
        } else if (type_ == UNSIGNED_SHORT) {
            maxIntensity = static_cast<float>(
                (std::numeric_limits<unsigned short>::max)());
        } else if (type_ == INT32) {
            maxIntensity =
                static_cast<float>((std::numeric_limits<int>::max)());
        } else if (type_ == FLOAT32 || type_ == FLOAT64) {
            maxIntensity = 1.0f;
        }
        const auto upp_minus_low = upper[c] - lowest[c];

        if (upp_minus_low == 0.0f) {
            // Constant channel: auto-contrast cannot stretch a single value,
            // so leave it unchanged (identity: contrast 1, brightness 0)
            // instead of crushing it to zero. Otherwise a constant channel
            // -- e.g. a fully-opaque alpha (255) or a flat color plane --
            // would map to 0, rendering the buffer transparent/black.
            auto_buffer_contrast[c] = 1.0f;
            auto_buffer_brightness[c] = 0.0f;
            continue;
        }

        auto_buffer_contrast[c] = maxIntensity / upp_minus_low;
        auto_buffer_brightness[c] =
            -lowest[c] / maxIntensity * auto_buffer_contrast[c];
    }
    for (int c = channels_; c < 4; ++c) {
        auto_buffer_contrast[c] = auto_buffer_contrast[0];
        auto_buffer_brightness[c] = auto_buffer_brightness[0];
    }
}

int Buffer::sub_texture_id_at_coord(const int x, const int y) const {
    const auto tx = x / max_texture_size;
    const auto ty = y / max_texture_size;
    return static_cast<int>(buff_tex_[ty * num_textures_x_ + tx]);
}

void Buffer::configure(const BufferParams& params) {
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

    // Validate buffer size (prevent potential DoS). The real memory footprint
    // is the size of the received pixel data. `step` is in pixels per row (see
    // GL_UNPACK_ROW_LENGTH below), so a width*height*step estimate would
    // double-count the row width and reject valid large buffers. Validate the
    // actual byte count against the same 16 GB ceiling the desktop build uses.
    // Cast to uint64 so the comparison is well-formed on 32-bit builds, where
    // size_t cannot represent the 16 GB ceiling (this ceiling only ever binds
    // on the 64-bit desktop build).
    if (const auto buffer_size =
            static_cast<std::uint64_t>(params.buffer.size());
        buffer_size > BufferConstants::MAX_BUFFER_SIZE) {
        std::cerr << "[Error] Buffer size too large: " << buffer_size
                  << " bytes (maximum: " << BufferConstants::MAX_BUFFER_SIZE
                  << " bytes)" << std::endl;
        return;
    }

    buffer_ = params.buffer;
    channels_ = params.channels;
    type_ = params.type;
    buffer_width_f_ = static_cast<float>(params.buffer_width_i);
    buffer_height_f_ = static_cast<float>(params.buffer_height_i);
    step_ = params.step;
    transpose_ = params.transpose_buffer;
    // Only update pixel layout during initial setup, not on buffer updates
    // This preserves user-selected pixel formats when buffer updates
    if (buff_tex_.empty() && !params.pixel_layout.empty() &&
        params.pixel_layout.size() == 4) {
        set_pixel_layout(params.pixel_layout);
    }
    // Otherwise, pixel_layout_ remains unchanged, preserving user selection
}

void Buffer::set_pixel_layout(const std::string& pixel_layout) {
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
    // Use std::min to ensure we never write beyond pixel_layout_.size()
    const auto copy_size =
        (std::min)(pixel_layout.size(), pixel_layout_.size());
    for (int i = 0; i < static_cast<int>(copy_size); ++i) {
        pixel_layout_[i] = pixel_layout[i];
    }

    // Recreate shader program with new pixel layout
    create_shader_program();
}

const char* Buffer::get_pixel_layout() const {
    return pixel_layout_.data();
}

void Buffer::set_display_channel_mode(const int display_channels) {
    // Valid values: -1 (use actual buffer channels) or 1-4 (specific channel
    // count) Reject 0 and values outside valid range
    if (display_channels != -1 &&
        (display_channels < 1 || display_channels > 4)) {
        std::cerr << "[Error] Invalid display channel mode: "
                  << display_channels << " (must be -1 or between 1 and 4)"
                  << std::endl;
        return;
    }

    display_channel_mode_ = display_channels;
    create_shader_program();
}

int Buffer::get_display_channel_mode() const {
    return display_channel_mode_;
}

int Buffer::get_selected_channel_index() const {
    if (pixel_layout_[0] == 'g') {
        return 1;
    }
    if (pixel_layout_[0] == 'b') {
        return 2;
    }
    return 0;
}

float Buffer::tile_coord_x(const int x) const {
    const auto buffer_width_i = static_cast<int>(buffer_width_f_);
    const auto last_width = buffer_width_i % max_texture_size;
    const auto tile_width =
        x > buffer_width_i - last_width ? last_width : max_texture_size;
    return static_cast<float>(x % max_texture_size) /
           static_cast<float>(tile_width - 1);
}

float Buffer::tile_coord_y(const int y) const {
    const auto buffer_height_i = static_cast<int>(buffer_height_f_);
    const auto last_height = buffer_height_i % max_texture_size;
    const auto tile_height =
        y > buffer_height_i - last_height ? last_height : max_texture_size;
    return static_cast<float>(y % max_texture_size) /
           static_cast<float>(tile_height - 1);
}

void Buffer::update() {
    const auto stage = game_object_ref().get_stage();
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
    const auto zoom = camera.compute_zoom();

    buff_prog_.use();
    if (zoom > BufferConstants::ZOOM_BORDER_THRESHOLD) {
        buff_prog_.uniform1i("enable_borders", 1);
    } else {
        buff_prog_.uniform1i("enable_borders", 0);
    }

    update_object_pose();
}

void Buffer::update_object_pose() const {
    const auto rotation = mat4::rotation(angle_);

    auto transposition = mat4{};

    if (transpose_) {
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

    game_object_ref().set_pose(rotation * transposition);
}

bool Buffer::create_shader_program() {
    // Buffer Shaders
    // Use display_channel_mode_ if set, otherwise use actual buffer channels
    const auto effective_channels =
        (display_channel_mode_ == -1) ? channels_ : display_channel_mode_;

    // Validate effective_channels to ensure it's within supported range
    if (effective_channels < 1 || effective_channels > 4) {
        std::cerr << "[Error] Invalid effective channel count: "
                  << effective_channels
                  << " (must be between 1 and 4). Display mode: "
                  << display_channel_mode_ << ", buffer channels: " << channels_
                  << std::endl;
        return false;
    }

    auto channel_type = ShaderProgram::TexelChannels{};
    if (effective_channels == 1) {
        channel_type = ShaderProgram::TexelChannels::FORMAT_R;
    } else if (effective_channels == 2) {
        channel_type = ShaderProgram::TexelChannels::FORMAT_RG;
    } else if (effective_channels == 3) {
        channel_type = ShaderProgram::TexelChannels::FORMAT_RGB;
    } else {
        channel_type = ShaderProgram::TexelChannels::FORMAT_RGBA;
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

bool Buffer::initialize() {
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

    gl_canvas_ref().glGenBuffers(1, &vbo_);
    gl_canvas_ref().glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl_canvas_ref().glBufferData(GL_ARRAY_BUFFER,
                                 sizeof(g_vertex_buffer_data),
                                 g_vertex_buffer_data.data(),
                                 GL_STATIC_DRAW);

    setup_gl_buffer();

    update_object_pose();

    return true;
}

void Buffer::draw(const mat4& projection, const mat4& viewInv) {
    buff_prog_.use();
    const auto model = game_object_ref().get_pose();
    const auto mvp = projection * viewInv * model;

    gl_canvas_ref().glEnableVertexAttribArray(0);
    gl_canvas_ref().glActiveTexture(GL_TEXTURE0);

    buff_prog_.uniform1i("sampler", 0);
    if (const auto stage = game_object_ref().get_stage();
        stage.has_value() && stage->get().get_contrast_enabled()) {
        buff_prog_.uniform4fv(
            "brightness_contrast", 2, auto_buffer_contrast_brightness_.data());
    } else {
        buff_prog_.uniform4fv("brightness_contrast", 2, no_ac_params.data());
    }

    const auto buffer_width_i = static_cast<int>(buffer_width_f_);
    const auto buffer_height_i = static_cast<int>(buffer_height_f_);

    auto remaining_h = buffer_height_i;

    auto py = static_cast<float>(-buffer_height_i) / 2.0f;
    if (buffer_height_i % 2 == 1) {
        py -= 0.5f;
    }

    for (int ty = 0; ty < num_textures_y_; ++ty) {
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

        for (int tx = 0; tx < num_textures_x_; ++tx) {
            const auto buff_w = (std::min)(remaining_w, max_texture_size);
            remaining_w -= buff_w;

            gl_canvas_ref().glBindTexture(GL_TEXTURE_2D,
                                          buff_tex_[ty * num_textures_x_ + tx]);

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

            gl_canvas_ref().glBindBuffer(GL_ARRAY_BUFFER, vbo_);
            gl_canvas_ref().glVertexAttribPointer(
                0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            gl_canvas_ref().glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        py += static_cast<float>(buff_h) / 2.0f;
    }
}

const std::vector<GLuint>& Buffer::buff_tex() const {
    return buff_tex_;
}

float Buffer::buffer_width_f() const {
    return buffer_width_f_;
}

float Buffer::buffer_height_f() const {
    return buffer_height_f_;
}

int Buffer::channels() const {
    return channels_;
}

int Buffer::step() const {
    return step_;
}

BufferType Buffer::type() const {
    return type_;
}

std::span<const std::byte> Buffer::buffer() const {
    return buffer_;
}

bool Buffer::transpose() const {
    return transpose_;
}

int Buffer::num_textures_x() const {
    return num_textures_x_;
}

int Buffer::num_textures_y() const {
    return num_textures_y_;
}

std::span<float> Buffer::min_buffer_values() {
    return min_buffer_values_;
}

std::span<float> Buffer::max_buffer_values() {
    return max_buffer_values_;
}

std::span<const float> Buffer::max_buffer_values() const {
    return max_buffer_values_;
}

const float* Buffer::auto_buffer_contrast_brightness() const {
    return auto_buffer_contrast_brightness_.data();
}

void Buffer::setup_gl_buffer() {
    const auto buffer_width_i = static_cast<int>(buffer_width_f_);
    const auto buffer_height_i = static_cast<int>(buffer_height_f_);

    // Initialize contrast parameters
    reset_contrast_brightness_parameters();

    // Buffer texture
    constexpr auto max_texture_size_f = static_cast<float>(max_texture_size);
    num_textures_x_ = static_cast<int>(
        std::ceil(static_cast<float>(buffer_width_i) / max_texture_size_f));
    num_textures_y_ = static_cast<int>(
        std::ceil(static_cast<float>(buffer_height_i) / max_texture_size_f));
    const int num_textures = num_textures_x_ * num_textures_y_;

    buff_tex_.resize(num_textures);
    gl_canvas_ref().glGenTextures(num_textures, buff_tex_.data());

    auto tex_type = GLuint{GL_UNSIGNED_BYTE};
    auto tex_format = GLuint{GL_RED};

    if (type_ == BufferType::FLOAT32 || type_ == BufferType::FLOAT64) {
        tex_type = GL_FLOAT;
    } else if (type_ == BufferType::UNSIGNED_BYTE) {
        tex_type = GL_UNSIGNED_BYTE;
    } else if (type_ == BufferType::SHORT) {
        tex_type = GL_SHORT;
    } else if (type_ == BufferType::UNSIGNED_SHORT) {
        tex_type = GL_UNSIGNED_SHORT;
    } else if (type_ == BufferType::INT32) {
        tex_type = GL_INT;
    }

    if (channels_ == 1) {
        tex_format = GL_RED;
    } else if (channels_ == 2) {
        tex_format = GL_RG;
    } else if (channels_ == 3) {
        tex_format = GL_RGB;
    } else if (channels_ == 4) {
        tex_format = GL_RGBA;
    }

    const auto internal_format =
        the_dialect().texture_internal_format(tex_type, tex_format);

    auto remaining_h = buffer_height_i;

    gl_canvas_ref().glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl_canvas_ref().glPixelStorei(GL_UNPACK_ROW_LENGTH, step_);

    for (int ty = 0; ty < num_textures_y_; ++ty) {
        const auto buff_h = (std::min)(remaining_h, max_texture_size);
        remaining_h -= buff_h;

        auto remaining_w = buffer_width_i;
        for (int tx = 0; tx < num_textures_x_; ++tx) {
            const auto buff_w = (std::min)(remaining_w, max_texture_size);
            remaining_w -= buff_w;

            const auto tex_id = ty * num_textures_x_ + tx;
            gl_canvas_ref().glBindTexture(GL_TEXTURE_2D, buff_tex_[tex_id]);

            gl_canvas_ref().glPixelStorei(GL_UNPACK_SKIP_ROWS,
                                          ty * max_texture_size);
            gl_canvas_ref().glPixelStorei(GL_UNPACK_SKIP_PIXELS,
                                          tx * max_texture_size);

            gl_canvas_ref().glTexImage2D(GL_TEXTURE_2D,
                                         0,
                                         static_cast<GLint>(internal_format),
                                         buff_w,
                                         buff_h,
                                         0,
                                         tex_format,
                                         tex_type,
                                         nullptr);

            gl_canvas_ref().glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                buff_w,
                buff_h,
                tex_format,
                tex_type,
                std::bit_cast<const GLvoid*>(buffer_.data()));

            gl_canvas_ref().glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            gl_canvas_ref().glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            gl_canvas_ref().glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl_canvas_ref().glTexParameteri(
                GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            if (the_dialect().has_texture_wrap_r) {
                gl_canvas_ref().glTexParameteri(
                    GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            }
        }
    }

    gl_canvas_ref().glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    gl_canvas_ref().glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    gl_canvas_ref().glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
}

} // namespace oid
