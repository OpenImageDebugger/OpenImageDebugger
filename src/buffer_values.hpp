#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>

#include "shader.hpp"
#include "component.hpp"

class BufferValues : public Component {
public:
    static constexpr float font_size = 96.0f;

    ~BufferValues();

    bool initialize();

    void update() {
    }

    void draw(const mat4& projection, const mat4& viewInv);
private:
    GLuint text_tex;
    GLuint text_vbo;
    FT_Face font;
    FT_Library ft;
    ShaderProgram text_prog;
    float text_pixel_scale = 1.0;
    float text_texture_width, text_texture_height;
    int text_texture_offsets[256][2];
    int text_texture_advances[256][2];
    int text_texture_sizes[256][2];
    int text_texture_tls[256][2];

    void generate_glyphs_texture();

    void draw_text(const mat4& projection, const mat4& viewInv,
            const char* text, float x, float y, float y_offset, float scale = 1.0f);
};

