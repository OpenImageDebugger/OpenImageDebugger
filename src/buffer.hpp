#pragma once

#include <vector>
#include "shader.hpp"
#include "component.hpp"

using namespace std;
class Buffer : public Component {
public:
    int max_texture_size = 2048;

    std::vector<GLuint> buff_tex;
    static const float no_ac_params[6];

    enum class BufferType { Float32 = 0, UnsignedByte = 1, Short = 2, UnsignedShort = 3, Int32 = 4, UnsignedInt32 = 5 };

    ~Buffer();

    float buffer_width_f;
    float buffer_height_f;
    int channels;
    BufferType type;
    int step;
    uint8_t* buffer;

    bool buffer_update() {
        int num_textures = num_textures_x*num_textures_y;
        glDeleteTextures(num_textures, buff_tex.data());

        setup_gl_buffer();
        return true;
    }

    void recomputeMinColorValues() {
        int buffer_width_i = static_cast<int>(buffer_width_f);
        int buffer_height_i = static_cast<int>(buffer_height_f);

        float *lowest = min_buffer_values();
        for(int i = 0; i < 3; ++i)
            lowest[i] = std::numeric_limits<float>::max();

        for(int y = 0; y < buffer_height_i; ++y) {
            for(int x = 0; x < buffer_width_i; ++x) {
                int i = y*step + x;
                for(int c = 0; c < channels; ++c) {
                    if(type == BufferType::Float32)
                        lowest[c] = std::min(lowest[c],
                                reinterpret_cast<float*>(buffer)[channels*i + c]);
                    else if(type == BufferType::UnsignedByte)
                        lowest[c] = std::min(lowest[c],
                                static_cast<float>(buffer[channels*i + c]));
                    else if(type == BufferType::Short)
                        lowest[c] = std::min(lowest[c],
                                static_cast<float>(reinterpret_cast<short*>(buffer)[channels*i + c]));
                    else if(type == BufferType::UnsignedShort)
                        lowest[c] = std::min(lowest[c],
                                static_cast<float>(reinterpret_cast<unsigned short*>(buffer)[channels*i + c]));
                    else if(type == BufferType::Int32)
                        lowest[c] = std::min(lowest[c],
                                static_cast<float>(reinterpret_cast<int*>(buffer)[channels*i + c]));
                    else if(type == BufferType::UnsignedInt32)
                        lowest[c] = std::min(lowest[c],
                                static_cast<float>(reinterpret_cast<unsigned int*>(buffer)[channels*i + c]));
                }
            }
        }

        // For single channel buffers: fill with 0
        for(int c = channels; c < 3; ++c)
            lowest[c] = 0.0;
    }

    void recomputeMaxColorValues() {
        int buffer_width_i = static_cast<int>(buffer_width_f);
        int buffer_height_i = static_cast<int>(buffer_height_f);

        float *upper = max_buffer_values();
        for(int i = 0; i < 3; ++i)
            upper[i] = std::numeric_limits<float>::lowest();

        for(int y = 0; y < buffer_height_i; ++y) {
            for(int x = 0; x < buffer_width_i; ++x) {
                int i = y*step + x;
                for(int c = 0; c < channels; ++c) {
                    if(type == BufferType::Float32)
                        upper[c] = std::max(upper[c],
                                reinterpret_cast<float*>(buffer)[channels*i + c]);
                    else if(type == BufferType::UnsignedByte)
                        upper[c] = std::max(upper[c],
                                static_cast<float>(buffer[channels*i + c]));
                    else if(type == BufferType::Short)
                        upper[c] = std::max(upper[c],
                                static_cast<float>(reinterpret_cast<short*>(buffer)[channels*i + c]));
                    else if(type == BufferType::UnsignedShort)
                        upper[c] = std::max(upper[c],
                                static_cast<float>(reinterpret_cast<unsigned short*>(buffer)[channels*i + c]));
                    else if(type == BufferType::Int32)
                        upper[c] = std::max(upper[c],
                                static_cast<float>(reinterpret_cast<int*>(buffer)[channels*i + c]));
                    else if(type == BufferType::UnsignedInt32)
                        upper[c] = std::max(upper[c],
                                static_cast<float>(reinterpret_cast<unsigned int*>(buffer)[channels*i + c]));
                }
            }
        }

        // For single channel buffers: fill with 0
        for(int c = channels; c < 3; ++c)
            upper[c] = 0.0;
    }

    void resetContrastBrightnessParameters() {
        recomputeMinColorValues();
        recomputeMaxColorValues();

        computeContrastBrightnessParameters();
    }

    void computeContrastBrightnessParameters() {
        float *lowest = min_buffer_values();
        float *upper = max_buffer_values();

        float* auto_buffer_contrast = auto_buffer_contrast_brightness_;
        float* auto_buffer_brightness = auto_buffer_contrast_brightness_+3;

        for(int c = 0; c < channels; ++c) {
            float maxIntensity = 1.0f;
            if(type == BufferType::Float32)
                maxIntensity = 1.0f;
            else // All non-real values have max color 255
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

    bool initialize();

    void update();

    void draw(const mat4& projection, const mat4& viewInv);

    int num_textures_x;
    int num_textures_y;

    float* min_buffer_values() {
        return min_buffer_values_;
    }

    float* max_buffer_values() {
        return max_buffer_values_;
    }

    float* auto_buffer_contrast_brightness() {
        return auto_buffer_contrast_brightness_;
    }

    void set_min_buffer_values();
    void set_max_buffer_values();

private:
    void setup_gl_buffer();
    float min_buffer_values_[3];
    float max_buffer_values_[3];
    float auto_buffer_contrast_brightness_[6] = {1.0,1.0,1.0, 0.0,0.0,0.0};

    ShaderProgram buff_prog;
    GLuint vbo;
};

