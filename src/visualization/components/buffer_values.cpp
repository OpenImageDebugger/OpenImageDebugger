/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2023 OpenImageDebugger contributors
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

#include "buffer_values.h"

#include <algorithm>
#include <array>


#include <QFontMetrics>


#include "buffer.h"
#include "camera.h"
#include "visualization/game_object.h"
#include "visualization/stage.h"


using namespace std;


BufferValues::BufferValues(GameObject* game_object, GLCanvas* gl_canvas)
    : Component(game_object, gl_canvas)
{
}


BufferValues::~BufferValues()
{
}


int BufferValues::render_index() const
{
    return 50;
}


inline void pix2str(const BufferType& type,
                    const uint8_t* buffer,
                    const int& pos,
                    const int& channel,
                    const int label_length,
                    char* pix_label)
{
    if (type == BufferType::Float32 || type == BufferType::Float64) {
        float fpix = reinterpret_cast<const float*>(buffer)[pos + channel];
        snprintf(pix_label, label_length, "%.3f", fpix);
        if (strlen(pix_label) > 7)
            snprintf(pix_label, label_length, "%.3e", fpix);
    } else if (type == BufferType::UnsignedByte) {
        snprintf(pix_label, label_length, "%d", buffer[pos + channel]);
    } else if (type == BufferType::Short) {
        short fpix = reinterpret_cast<const short*>(buffer)[pos + channel];
        snprintf(pix_label, label_length, "%d", fpix);
    } else if (type == BufferType::UnsignedShort) {
        unsigned short fpix =
            reinterpret_cast<const unsigned short*>(buffer)[pos + channel];
        snprintf(pix_label, label_length, "%d", fpix);
    } else if (type == BufferType::Int32) {
        int fpix = reinterpret_cast<const int*>(buffer)[pos + channel];
        snprintf(pix_label, label_length, "%d", fpix);
        if (strlen(pix_label) > 7)
            snprintf(pix_label, label_length, "%.3e", static_cast<float>(fpix));
    }
}


void BufferValues::draw(const mat4& projection, const mat4& view_inv)
{
    GameObject* cam_obj = game_object_->stage->get_game_object("camera");
    Camera* camera      = cam_obj->get_component<Camera>("camera_component");
    float zoom          = camera->compute_zoom();

    if (zoom > 40) {
        mat4 buffer_pose = game_object_->get_pose();

        Buffer* buffer_component =
            game_object_->get_component<Buffer>("buffer_component");
        float buffer_width_f  = buffer_component->buffer_width_f;
        float buffer_height_f = buffer_component->buffer_height_f;
        int step              = buffer_component->step;
        int channels          = buffer_component->channels;
        BufferType type       = buffer_component->type;
        const uint8_t* buffer = buffer_component->buffer;

        vec4 tl_ndc(-1, 1, 0, 1);
        vec4 br_ndc(1, -1, 0, 1);
        mat4 vp_inv = (projection * view_inv * buffer_pose).inv();
        vec4 tl     = vp_inv * tl_ndc;
        vec4 br     = vp_inv * br_ndc;

        // Since the clip ROI may be rotated, we need to re-compute TL and BR
        // from their Xs and Ys
        int tl_x = min(tl.x(), br.x()), tlY = min(tl.y(), br.y());
        int br_x = max(tl.x(), br.x()), brY = max(tl.y(), br.y());
        tl.x() = tl_x, tl.y() = tlY;
        br.x() = br_x, br.y() = brY;


        int lower_x = clamp(truncf(tl.x()) - 1.0f,
                            -buffer_width_f / 2.0f,
                            buffer_width_f / 2.0f - 1.f);
        int upper_x = clamp(ceilf(br.x()) + 1.f,
                            -(buffer_width_f + 1) / 2.0f + 1.f,
                            (buffer_width_f + 1) / 2.0f);

        int lower_y = clamp(truncf(tl.y()) - 1.0f,
                            -buffer_height_f / 2.0f,
                            buffer_height_f / 2.0f - 1.0f);
        int upper_y = clamp(ceilf(br.y()) + 1.f,
                            -(buffer_height_f + 1) / 2.0f + 1.f,
                            (buffer_height_f + 1) / 2.0f);

        int pos_center_x = -buffer_width_f / 2;
        int pos_center_y = -buffer_height_f / 2;

        const int label_length = 30;
        char pix_label[label_length];

        int pos;
        float y_off;

        // Offset for vertical channel position to account for padding
        array<float, 4> recenter_factors;

        if (channels == 1) {
            recenter_factors = {0.f, 0.f, 0.f, 0.f};
        } else if (channels == 2) {
            float rfUp       = padding / 3.0 / channels;
            recenter_factors = {rfUp, -rfUp, 0.f, 0.f};
        } else if (channels == 3) {
            float rfUp       = padding / 2.0 / channels;
            recenter_factors = {rfUp, 0.f, -rfUp, 0.f};
        } else if (channels == 4) {
            float rfUp       = 3.f * padding / 5.f / channels;
            float rfDown     = padding / 5.f / channels;
            recenter_factors = {rfUp, rfDown, -rfDown, -rfUp};
        }

        for (int y = lower_y - pos_center_y; y < upper_y - pos_center_y; ++y) {
            for (int x = lower_x - pos_center_x; x < upper_x - pos_center_x;
                 ++x) {
                pos = (y * step + x) * channels;

                for (int c = 0; c < channels; ++c) {
                    y_off = (0.5f * (channels - 1) - c) / channels -
                            recenter_factors[c];

                    pix2str(type, buffer, pos, c, label_length, pix_label);
                    draw_text(projection,
                              view_inv,
                              buffer_pose,
                              pix_label,
                              x + pos_center_x,
                              y + pos_center_y,
                              y_off,
                              channels);
                }
            }
        }
    }
}


void BufferValues::draw_text(const mat4& projection,
                             const mat4& view_inv,
                             const mat4& buffer_pose,
                             const char* text,
                             float x,
                             float y,
                             float y_offset,
                             float channels)
{
    const GLTextRenderer* text_renderer = gl_canvas_->get_text_renderer();

    Buffer* buffer_component =
        game_object_->get_component<Buffer>("buffer_component");

    const float* auto_buffer_contrast_brightness;

    if (game_object_->stage->contrast_enabled) {
        auto_buffer_contrast_brightness =
            buffer_component->auto_buffer_contrast_brightness();
    } else {
        auto_buffer_contrast_brightness = Buffer::no_ac_params;
    }

    text_renderer->text_prog.use();
    gl_canvas_->glEnableVertexAttribArray(0);
    gl_canvas_->glBindBuffer(GL_ARRAY_BUFFER, text_renderer->text_vbo);
    gl_canvas_->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);

    gl_canvas_->glActiveTexture(GL_TEXTURE0);
    GLuint buff_tex = buffer_component->sub_texture_id_at_coord(
        x + buffer_component->buffer_width_f / 2.f,
        y + buffer_component->buffer_height_f / 2.f);
    glBindTexture(GL_TEXTURE_2D, buff_tex);
    text_renderer->text_prog.uniform1i("buff_sampler", 0);

    gl_canvas_->glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, text_renderer->text_tex);
    text_renderer->text_prog.uniform1i("text_sampler", 1);

    text_renderer->text_prog.uniform_matrix4fv(
        "mvp", 1, GL_FALSE, (projection * view_inv).data());
    text_renderer->text_prog.uniform2f(
        "pix_coord",
        buffer_component->tile_coord_x(x +
                                       buffer_component->buffer_width_f / 2.f),
        buffer_component->tile_coord_y(y + buffer_component->buffer_height_f /
                                               2.f));

    text_renderer->text_prog.uniform4fv(
        "brightness_contrast", 2, auto_buffer_contrast_brightness);

    // Compute text box size
    float boxW = 0, boxH = 0;
    for (auto p = reinterpret_cast<const unsigned char*>(text); *p; p++) {
        boxW += text_renderer->text_texture_advances[*p][0];
        boxH = max(boxH, (float)text_renderer->text_texture_sizes[*p][1]);
    }

    float paddingScale = 1.f / (1.f - 2.f * padding);
    text_pixel_scale =
        max(text_pixel_scale, max(boxW, boxH) * paddingScale * channels);
    float sx = 1.0 / text_pixel_scale;
    float sy = 1.0 / text_pixel_scale;

    vec4 channel_offset(0, y_offset, 0, 0);

    vec4 centeredCoord(x, y, 0, 1);

    if (static_cast<int>(buffer_component->buffer_width_f) % 2 == 0) {
        centeredCoord.x() += 0.5f;
    }
    if (static_cast<int>(buffer_component->buffer_height_f) % 2 == 0) {
        centeredCoord.y() += 0.5f;
    }

    centeredCoord = buffer_pose * centeredCoord;

    y = centeredCoord.y() + boxH / 2.0 * sy - channel_offset.y();
    x = centeredCoord.x() - boxW / 2.0 * sx - channel_offset.x();

    for (auto p = reinterpret_cast<const unsigned char*>(text); *p; p++) {
        float x2 = x + text_renderer->text_texture_tls[*p][0] * sx;
        float y2 = y - text_renderer->text_texture_tls[*p][1] * sy;

        int tex_wid = text_renderer->text_texture_sizes[*p][0];
        int tex_hei = text_renderer->text_texture_sizes[*p][1];

        float w = tex_wid * sx;
        float h = tex_hei * sy;

        float tex_lower_x =
            ((float)text_renderer->text_texture_offsets[*p][0]) /
            text_renderer->text_texture_width;
        float tex_lower_y =
            ((float)text_renderer->text_texture_offsets[*p][1]) /
            text_renderer->text_texture_height;
        float tex_upper_x = tex_lower_x + ((float)tex_wid - 1.0f) /
                                              text_renderer->text_texture_width;
        float tex_upper_y =
            tex_lower_y +
            ((float)tex_hei - 1.0f) / text_renderer->text_texture_height;

        /*
         * box format: <pixel coord x, pixel coord y, texture coord x, texture
         * coord y>
         */
        GLfloat box[4][4] = {
            {x2, y2, tex_lower_x, tex_lower_y},
            {x2 + w, y2, tex_upper_x, tex_lower_y},
            {x2, y2 + h, tex_lower_x, tex_upper_y},
            {x2 + w, y2 + h, tex_upper_x, tex_upper_y},
        };

        gl_canvas_->glBufferData(
            GL_ARRAY_BUFFER, sizeof box, box, GL_DYNAMIC_DRAW);
        gl_canvas_->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        vec4 char_step_direction(
            text_renderer->text_texture_advances[*p][0] * sx,
            text_renderer->text_texture_advances[*p][1] * sy,
            0.0,
            1.0);

        x += char_step_direction.x();
        y += char_step_direction.y();
    }
}
