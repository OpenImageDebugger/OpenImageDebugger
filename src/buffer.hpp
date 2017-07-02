#pragma once

#include <vector>
#include <sstream>
#include "shader.hpp"
#include "component.hpp"

using namespace std;
class Buffer : public Component {
public:
    int max_texture_size = 2048;

    std::vector<GLuint> buff_tex;
    static const float no_ac_params[8];

    enum class BufferType { UnsignedByte = 0, UnsignedShort = 2, Short = 3, Int32 = 4, Float32 = 5, Float64 = 6 };

    ~Buffer();

    float buffer_width_f;
    float buffer_height_f;
    int channels;
    BufferType type;
    int step;
    uint8_t* buffer;

    bool buffer_update();

    void recomputeMinColorValues();

    void recomputeMaxColorValues();

    void resetContrastBrightnessParameters();

    void computeContrastBrightnessParameters();

    int sub_texture_id_at_coord(int x, int y);

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

    void getPixelInfo(stringstream& output, int x, int y);
private:
    void create_shader_program();
    void setup_gl_buffer();

    float min_buffer_values_[4];
    float max_buffer_values_[4];
    float auto_buffer_contrast_brightness_[8] = {1.0,1.0,1.0,1.0, 0.0,0.0,0.0,0.0};

    ShaderProgram buff_prog;
    GLuint vbo;
};

