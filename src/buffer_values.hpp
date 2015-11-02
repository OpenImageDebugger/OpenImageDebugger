#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>

#include "shader.hpp"
#include "component.hpp"

class BufferValues : public Component {
public:
    static constexpr float font_size = 96.0f;

    ~BufferValues() {
        glDeleteTextures(1, &text_tex);
        glDeleteBuffers(1, &text_vbo);
    }

    bool initialize() {
        if(FT_Init_FreeType(&ft)) {
            std::cerr << "Failed to initialize freetype" << std::endl;
            return false;
        }

        // The macro FONT_PATH is defined at compile time.
        if(FT_New_Face(ft, FONT_PATH, 0, &font)) {
            std::cerr << "Could not open font " FONT_PATH << std::endl;
            return false;
        }
        FT_Set_Pixel_Sizes(font, 0, font_size);

        text_prog.create(shader::text_vert_shader,
                shader::text_frag_shader,
                ShaderProgram::Grayscale, {
                "mvp",
                "buff_sampler",
                "text_sampler",
                "pix_coord",
                "brightness_contrast"
                });

        glGenTextures(1, &text_tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, text_tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glGenBuffers(1, &text_vbo);
        generate_glyphs_texture();

        return true;
    }

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

    void generate_glyphs_texture() {
        const char* text="0123456789., -e";
        const char *p;

        FT_GlyphSlot g = font->glyph;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, text_tex);

        // Compute text box size
        float boxW = 0, boxH = 0;
        for(p = text; *p; p++) {
            if(FT_Load_Char(font, *p, FT_LOAD_RENDER))
                continue;
            text_texture_advances[*p][0] = (g->advance.x >> 6);
            text_texture_advances[*p][1] = (g->advance.y >> 6);
            text_texture_sizes[*p][0] = g->bitmap.width;
            text_texture_sizes[*p][1] = g->bitmap.rows;
            text_texture_tls[*p][0] = g->bitmap_left;
            text_texture_tls[*p][1] = g->bitmap_top;
            boxW += g->bitmap.width;
            boxH = std::max(boxH, (float)g->bitmap.rows);
        }

        text_texture_width = text_texture_height = 1.0f;
        while(text_texture_width<boxW) text_texture_width *= 2.f;
        while(text_texture_height<boxH) text_texture_height *= 2.f;

        const int mipmapLevels = 5;
        glTexStorage2D(GL_TEXTURE_2D, mipmapLevels, GL_R8, text_texture_width, text_texture_height);

        // Clears generated buffer
        {
            std::vector<uint8_t> zeros(text_texture_width*text_texture_height, 0);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, text_texture_width, text_texture_height, GL_RED, GL_UNSIGNED_BYTE, zeros.data());
        }

        int x = 0, y = 0;
        for(p = text; *p; p++) {
            if(FT_Load_Char(font, *p, FT_LOAD_RENDER))
                continue;

            glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, g->bitmap.width, g->bitmap.rows, GL_RED, GL_UNSIGNED_BYTE, g->bitmap.buffer);
            text_texture_offsets[*p][0] = x;
            text_texture_offsets[*p][1] = y;

            x += g->bitmap.width;
            y += (g->advance.y >> 6);
        }

        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    }

    void draw_text(const mat4& projection, const mat4& viewInv,
            const char* text, float x, float y, float scale = 1.0f);
};

