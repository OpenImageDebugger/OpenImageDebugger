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

#ifndef BUFFER_H_
#define BUFFER_H_

#include <sstream>
#include <vector>

#include "component.h"
#include "visualization/shader.h"
#include "ipc/message_exchange.h"


class Buffer final : public Component
{
  public:
    Buffer(GameObject* game_object, GLCanvas* gl_canvas);

    const int max_texture_size = 2048;

    std::vector<GLuint> buff_tex;

    static const float no_ac_params[8];

    float buffer_width_f{};
    float buffer_height_f{};

    int channels{};
    int step{};

    BufferType type;

    const uint8_t* buffer{};

    bool transpose{};

    ~Buffer() override;

    bool buffer_update() override;

    void recompute_min_color_values();

    void recompute_max_color_values();

    void reset_contrast_brightness_parameters();

    void compute_contrast_brightness_parameters();

    [[nodiscard]] int sub_texture_id_at_coord(int x, int y) const;

    void set_pixel_layout(const std::string& pixel_layout);

    [[nodiscard]] const char* get_pixel_layout() const;

    [[nodiscard]] float tile_coord_x(int x) const;
    [[nodiscard]] float tile_coord_y(int y) const;

    bool initialize() override;

    void update() override;

    void draw(const mat4& projection, const mat4& viewInv) override;

    int num_textures_x{};
    int num_textures_y{};

    float* min_buffer_values();

    float* max_buffer_values();

    [[nodiscard]] const float* auto_buffer_contrast_brightness() const;

    void set_min_buffer_values();
    void set_max_buffer_values();

    void get_pixel_info(std::stringstream& message, int x, int y) const;

    void rotate(float angle);

  private:
    void create_shader_program();

    void setup_gl_buffer();

    void update_object_pose() const;

    char pixel_layout_[4] = {'r', 'g', 'b', 'a'};

    float min_buffer_values_[4]{};
    float max_buffer_values_[4]{};
    float auto_buffer_contrast_brightness_[8] =
        {1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float angle_ = 0.0f;

    ShaderProgram buff_prog;
    GLuint vbo{};
};

#endif // BUFFER_H_
