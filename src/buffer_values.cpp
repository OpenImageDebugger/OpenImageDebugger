#include <GL/glew.h>
#define GLFW_INCLUDE_GL3
#define GLFW_NO_GLU
#include <GLFW/glfw3.h>

#include "buffer_values.hpp"
#include "stage.hpp"

using namespace std;

void BufferValues::draw(const mat4& projection, const mat4& viewInv) {
    float zoom = (stage->getComponent<Camera>("camera_component"))->zoom;
    if(zoom > 40) {
        Buffer* buffer_component = stage->getComponent<Buffer>("buffer_component");
        float buffer_width_f = buffer_component->buffer_width_f;
        float buffer_height_f = buffer_component->buffer_height_f;
        int buffer_width_i = static_cast<int>(buffer_width_f);
        int buffer_height_i = static_cast<int>(buffer_height_f);
        int channels = buffer_component->channels;
        int type = buffer_component->type;
        uint8_t* buffer = buffer_component->buffer;

        vec4 tl_ndc(-1,1,0,1);
        vec4 br_ndc(1,-1,0,1);
        mat4 vpInv = (projection*viewInv).inv();
        vec4 tl = vpInv*tl_ndc;
        vec4 br = vpInv*br_ndc;

        int lower_x = clamp(truncf(tl.x), 0.f, buffer_width_f-1.f);
        int upper_x = clamp(ceilf(br.x)+1.f, 1.f, buffer_width_f);

        int lower_y = clamp(-truncf(tl.y), 0.f, buffer_height_f-1.f);
        int upper_y = clamp(-floorf(br.y)+1.f, 1.f, buffer_height_f);

        for(int y = lower_y; y < upper_y; ++y) {
            for(int x = lower_x; x < upper_x; ++x) {
                char pix_label[100];
                int pos = y*buffer_width_i*channels + x*channels;
                if(channels == 1) {
                    if(type == 0)
                        sprintf(pix_label, "%.4f", reinterpret_cast<float*>(buffer)[pos]);
                    else if(type == 1)
                        sprintf(pix_label, "%d", buffer[pos]);

                    draw_text(projection, viewInv, pix_label, x, -y);
                } else if(channels==3) {
                    for(int c = 0; c < 3; ++c) {
                        float y_off = ((float)-c+1.0f)/5.0;
                        if(type == 0) {
                            sprintf(pix_label, "%.4f",
                                    reinterpret_cast<float*>(buffer)[pos+c]);
                            draw_text(projection, viewInv, pix_label, x, -y + y_off);
                        }
                        else if(type == 1) {
                            sprintf(pix_label, "%d", buffer[pos+c]);
                            draw_text(projection, viewInv, pix_label, x, -y + y_off, 0.5f);
                        }
                    }
                }
            }
        }
    }
}

void BufferValues::draw_text(const mat4& projection, const mat4& viewInv,
        const char* text, float x, float y, float scale) {
    Buffer* buffer_component = stage->getComponent<Buffer>("buffer_component");
    float buffer_width_f = buffer_component->buffer_width_f;
    float buffer_height_f = buffer_component->buffer_height_f;
    float* auto_buffer_contrast_brightness =
        buffer_component->auto_buffer_contrast_brightness;

    mat4 model;
    model.setIdentity();

    text_prog.use();
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);

    glActiveTexture(GL_TEXTURE0);
    GLuint buff_tex = buffer_component->buff_tex;
    glBindTexture(GL_TEXTURE_2D, buff_tex);
    text_prog.uniform1i("buff_sampler", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, text_tex);
    text_prog.uniform1i("text_sampler", 1);

    text_prog.uniformMatrix4fv("mvp", 1, GL_FALSE,
            (projection*viewInv*model).data);
    text_prog.uniform2f("pix_coord", x/(buffer_width_f-1.0f),
            (-y)/(buffer_height_f-1.0f));
    text_prog.uniform3fv("brightness_contrast", 2,
            auto_buffer_contrast_brightness);

    // Compute text box size
    float boxW = 0, boxH = 0;
    for(const char* p = text; *p; p++) {
        boxW += text_texture_advances[*p][0];
        boxH = max(boxH, (float)text_texture_sizes[*p][1]);
    }

    text_pixel_scale = max(text_pixel_scale, boxW*2.f/scale);
    float sx = 1.0/text_pixel_scale;
    float sy = 1.0/text_pixel_scale;

    x -= boxW/2.0*sx;
    y -= boxH/2.0*sy;

    for(const char* p = text; *p; p++) {
        float x2 =  x + text_texture_tls[*p][0] * sx;
        float y2 = -y - text_texture_tls[*p][1] * sy;

        int tex_wid = text_texture_sizes[*p][0];
        int tex_hei = text_texture_sizes[*p][1];
        float w = tex_wid * sx;
        float h = tex_hei * sy;

        float tex_lower_x = ((float)text_texture_offsets[*p][0])/text_texture_width;
        float tex_lower_y = ((float)text_texture_offsets[*p][1])/text_texture_height;
        float tex_upper_x = tex_lower_x + ((float)tex_wid-1.0f)/text_texture_width;
        float tex_upper_y = tex_lower_y + ((float)tex_hei-1.0f)/text_texture_height;

        GLfloat box[4][4] = {
            {x2,     -y2    , tex_lower_x, tex_lower_y},
            {x2 + w, -y2    , tex_upper_x, tex_lower_y},
            {x2,     -y2 - h, tex_lower_x, tex_upper_y},
            {x2 + w, -y2 - h, tex_upper_x, tex_upper_y},
        };

        glBufferData(GL_ARRAY_BUFFER, sizeof box, box, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        x += text_texture_advances[*p][0] * sx;
        y += text_texture_advances[*p][1] * sy;
    }
}
