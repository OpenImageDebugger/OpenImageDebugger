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

#ifndef BUFFER_H_
#define BUFFER_H_

#include <array>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "component.h"
#include "ipc/raw_data_decode.h"
#include "visualization/shader.h"

namespace oid
{

namespace BufferConstants
{
constexpr int MAX_TEXTURE_SIZE        = 2048;
constexpr float ZOOM_BORDER_THRESHOLD = 40.0f;
constexpr int MIN_BUFFER_DIMENSION    = 1;
constexpr int MAX_BUFFER_DIMENSION =
    131072; // 2^17 = 128K (closest power of 2 to 100k)
constexpr int MIN_CHANNELS = 1;
constexpr int MAX_CHANNELS = 4;
constexpr std::size_t MAX_BUFFER_SIZE =
    16ULL * 1024ULL * 1024ULL * 1024ULL; // 16GB
} // namespace BufferConstants

struct BufferParams
{
    std::span<const std::byte> buffer;
    int buffer_width_i;
    int buffer_height_i;
    int channels;
    BufferType type;
    int step;
    std::string pixel_layout;
    bool transpose_buffer;
};

class Buffer final : public Component
{
  public:
    Buffer(std::shared_ptr<GameObject> game_object,
           std::shared_ptr<GLCanvas> gl_canvas);

    ~Buffer() noexcept override;

    Buffer(const Buffer&) = delete;

    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&&) = delete;

    Buffer& operator=(Buffer&&) = delete;

    static constexpr int max_texture_size = BufferConstants::MAX_TEXTURE_SIZE;

    std::vector<GLuint> buff_tex{};

    static const std::array<float, 8> no_ac_params;

    float buffer_width_f{};
    float buffer_height_f{};

    int channels{};
    int step{};

    BufferType type{BufferType::UnsignedByte};

    std::span<const std::byte> buffer_{};

    bool transpose{};

    [[nodiscard]] bool buffer_update() override;

    void recompute_min_color_values();

    void recompute_max_color_values();

    void reset_contrast_brightness_parameters();

    void compute_contrast_brightness_parameters();

    [[nodiscard]] int sub_texture_id_at_coord(int x, int y) const;

    void set_pixel_layout(const std::string& pixel_layout);

    [[nodiscard]] const char* get_pixel_layout() const;

    void set_display_channel_mode(int display_channels);

    [[nodiscard]] int get_display_channel_mode() const;

    // Returns the channel index (0=R, 1=G, 2=B) based on current pixel layout
    [[nodiscard]] int get_selected_channel_index() const;

    void configure(const BufferParams& params);

    [[nodiscard]] float tile_coord_x(int x) const;
    [[nodiscard]] float tile_coord_y(int y) const;

    [[nodiscard]] bool initialize() override;

    void update() override;

    void draw(const mat4& projection, const mat4& viewInv) override;

    int num_textures_x{};
    int num_textures_y{};

    std::span<float> min_buffer_values();

    std::span<float> max_buffer_values();

    [[nodiscard]] std::span<const float> max_buffer_values() const;

    [[nodiscard]] const float* auto_buffer_contrast_brightness() const;

    void get_pixel_info(std::stringstream& message, int x, int y) const;

    void rotate(float angle);

    void set_icon_drawing_mode(bool is_enabled) const;

  private:
    bool create_shader_program();

    void setup_gl_buffer();

    void update_object_pose() const;

    void update_min_color_value(float* lowest, const int i, const int c) const;

    void update_max_color_value(float* upper, const int i, const int c) const;

    std::string pixel_layout_{'r', 'g', 'b', 'a'};

    // Override for display mode: -1 means use actual buffer channels
    int display_channel_mode_{-1};

    std::array<float, 4> min_buffer_values_{};
    std::array<float, 4> max_buffer_values_{};
    std::array<float, 8> auto_buffer_contrast_brightness_{1.0f,
                                                          1.0f,
                                                          1.0f,
                                                          1.0f,
                                                          0.0f,
                                                          0.0f,
                                                          0.0f,
                                                          0.0f};
    float angle_{0.0f};

    ShaderProgram buff_prog_;
    GLuint vbo_{};
};

} // namespace oid

#endif // BUFFER_H_
