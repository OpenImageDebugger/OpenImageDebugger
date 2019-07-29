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

#ifndef BUFFER_H_
#define BUFFER_H_

#include <sstream>
#include <vector>

#include "component.h"
#include "visualization/shader.h"

// TODO move somewhere more appropriate
enum class BufferType {
    UnsignedByte  = 0,
    UnsignedShort = 2,
    Short         = 3,
    Int32         = 4,
    Float32       = 5,
    Float64       = 6
};
///


class Buffer : public Component
{
  public:
    Buffer(GameObject* game_object, GLCanvas* gl_canvas);

    const int max_texture_size = 2048;

    std::vector<GLuint> buff_tex;

    static const float no_ac_params[8];

    float buffer_width_f;
    float buffer_height_f;

    int channels;
    int step;

    BufferType type;

    const uint8_t* buffer;

    bool transpose;

    ~Buffer();

    bool buffer_update();

    void recompute_min_color_values();

    void recompute_max_color_values();

    void reset_contrast_brightness_parameters();

    void compute_contrast_brightness_parameters();

    int sub_texture_id_at_coord(int x, int y);

    void set_pixel_layout(const std::string& pixel_layout);

    const char* get_pixel_layout() const;

    float tile_coord_x(int x);
    float tile_coord_y(int y);

    bool initialize();

    void update();

    void draw(const mat4& projection, const mat4& viewInv);

    int num_textures_x;
    int num_textures_y;

    float* min_buffer_values();

    float* max_buffer_values();

    const float* auto_buffer_contrast_brightness() const;

    void set_min_buffer_values();
    void set_max_buffer_values();

    void get_pixel_info(std::stringstream& output, int x, int y);

    void rotate(float angle);

  private:
    void create_shader_program();

    void setup_gl_buffer();

    void update_object_pose();

    char pixel_layout_[4] = {'r', 'g', 'b', 'a'};

    float min_buffer_values_[4];
    float max_buffer_values_[4];
    float auto_buffer_contrast_brightness_[8] =
        {1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0};
    float angle_ = 0.f;

    ShaderProgram buff_prog;
    GLuint vbo;
};

#endif // BUFFER_H_
