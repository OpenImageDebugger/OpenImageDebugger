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

#include "buffer_values.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>

#include "ui/gl_text_renderer.h"

#include "buffer.h"
#include "camera.h"
#include "visualization/game_object.h"
#include "visualization/stage.h"


using namespace std;

namespace oid
{

BufferValues::BufferValues(GameObject* game_object, GLCanvas* gl_canvas)
    : Component(game_object, gl_canvas)
{
}


BufferValues::~BufferValues() = default;


int BufferValues::render_index() const
{
    return 50;
}


inline void pix2str(const BufferType& type,
                    const uint8_t* buffer,
                    const int& pos,
                    const int& channel,
                    const int label_length,
                    const int float_precision,
                    char* pix_label)
{
    if (type == BufferType::Float32 || type == BufferType::Float64) {
        const float fpix =
            reinterpret_cast<const float*>(buffer)[pos + channel];
        snprintf(pix_label, label_length, "%.*f", float_precision, fpix);
    } else if (type == BufferType::UnsignedByte) {
        snprintf(pix_label, label_length, "%d", buffer[pos + channel]);
    } else if (type == BufferType::Short) {
        const short fpix =
            reinterpret_cast<const short*>(buffer)[pos + channel];
        snprintf(pix_label, label_length, "%d", fpix);
    } else if (type == BufferType::UnsignedShort) {
        const unsigned short fpix =
            reinterpret_cast<const unsigned short*>(buffer)[pos + channel];
        snprintf(pix_label, label_length, "%d", fpix);
    } else if (type == BufferType::Int32) {
        const int fpix = reinterpret_cast<const int*>(buffer)[pos + channel];
        snprintf(pix_label, label_length, "%d", fpix);
        if (string{pix_label}.length() > 7) {
            snprintf(pix_label, label_length, "%.3e", static_cast<float>(fpix));
        }
    }
}


void BufferValues::draw(const mat4& projection, const mat4& view_inv)
{
    GameObject* cam_obj = game_object_->stage->get_game_object("camera");
    const auto* camera  = cam_obj->get_component<Camera>("camera_component");
    const float zoom    = camera->compute_zoom();

    if (zoom > 40.0f) {
        const mat4 buffer_pose = game_object_->get_pose();

        const auto* buffer_component =
            game_object_->get_component<Buffer>("buffer_component");
        const float buffer_width_f  = buffer_component->buffer_width_f;
        const float buffer_height_f = buffer_component->buffer_height_f;
        const int step              = buffer_component->step;
        const int channels          = buffer_component->channels;
        const BufferType type       = buffer_component->type;
        const uint8_t* buffer       = buffer_component->buffer;

        const vec4 tl_ndc(-1.0f, 1.0f, 0.0f, 1.0f);
        const vec4 br_ndc(1.0f, -1.0f, 0.0f, 1.0f);
        const mat4 vp_inv = (projection * view_inv * buffer_pose).inv();
        vec4 tl           = vp_inv * tl_ndc;
        vec4 br           = vp_inv * br_ndc;

        // Since the clip ROI may be rotated, we need to re-compute TL and BR
        // from their Xs and Ys
        const int tl_x = min(tl.x(), br.x());
        const int tl_y = min(tl.y(), br.y());
        const int br_x = max(tl.x(), br.x());
        const int br_y = max(tl.y(), br.y());
        tl.x() = tl_x, tl.y() = tl_y;
        br.x() = br_x, br.y() = br_y;


        const int lower_x = clamp(truncf(tl.x()) - 1.0f,
                                  -buffer_width_f / 2.0f,
                                  buffer_width_f / 2.0f - 1.0f);
        const int upper_x = clamp(ceilf(br.x()) + 1.0f,
                                  -(buffer_width_f + 1.0f) / 2.0f + 1.0f,
                                  (buffer_width_f + 1.0f) / 2.0f);

        const int lower_y = clamp(truncf(tl.y()) - 1.0f,
                                  -buffer_height_f / 2.0f,
                                  buffer_height_f / 2.0f - 1.0f);
        const int upper_y = clamp(ceilf(br.y()) + 1.0f,
                                  -(buffer_height_f + 1.0f) / 2.0f + 1.0f,
                                  (buffer_height_f + 1.0f) / 2.0f);

        const int pos_center_x = -buffer_width_f / 2;
        const int pos_center_y = -buffer_height_f / 2;

        // Offset for vertical channel position to account for padding
        array<float, 4> recenter_factors{};

        if (channels == 1) {
            recenter_factors = {0.0f, 0.0f, 0.0f, 0.0f};
        } else if (channels == 2) {
            const auto rfUp  = padding / 3.0f / channels;
            recenter_factors = {rfUp, -rfUp, 0.0f, 0.0f};
        } else if (channels == 3) {
            const auto rfUp  = padding / 2.0f / channels;
            recenter_factors = {rfUp, 0.0f, -rfUp, 0.0f};
        } else if (channels == 4) {
            const auto rfUp   = 3.0f * padding / 5.0f / channels;
            const auto rfDown = padding / 5.0f / channels;
            recenter_factors  = {rfUp, rfDown, -rfDown, -rfUp};
        }

        for (int y = lower_y - pos_center_y; y < upper_y - pos_center_y; ++y) {
            for (int x = lower_x - pos_center_x; x < upper_x - pos_center_x;
                 ++x) {
                int pos = (y * step + x) * channels;

                for (int c = 0; c < channels; ++c) {
                    constexpr int label_length = 30;
                    char pix_label[label_length];
                    const float y_off = (0.5f * (channels - 1) - c) / channels -
                                        recenter_factors[c];

                    pix2str(type,
                            buffer,
                            pos,
                            c,
                            label_length,
                            float_precision,
                            pix_label);
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
                             const float y_offset,
                             const float channels)
{
    const GLTextRenderer* text_renderer = gl_canvas_->get_text_renderer();

    const auto* buffer_component =
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
    gl_canvas_->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    gl_canvas_->glActiveTexture(GL_TEXTURE0);
    const auto x_plus_half_buffer_width =
        static_cast<int>(x + buffer_component->buffer_width_f / 2.0f);
    const auto y_plus_half_buffer_height =
        static_cast<int>(y + buffer_component->buffer_height_f / 2.0f);

    const GLuint buff_tex = buffer_component->sub_texture_id_at_coord(
        x_plus_half_buffer_width, y_plus_half_buffer_height);
    glBindTexture(GL_TEXTURE_2D, buff_tex);
    text_renderer->text_prog.uniform1i("buff_sampler", 0);

    gl_canvas_->glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, text_renderer->text_tex);
    text_renderer->text_prog.uniform1i("text_sampler", 1);

    text_renderer->text_prog.uniform_matrix4fv(
        "mvp", 1, GL_FALSE, (projection * view_inv).data());
    text_renderer->text_prog.uniform2f(
        "pix_coord",
        buffer_component->tile_coord_x(x_plus_half_buffer_width),
        buffer_component->tile_coord_y(y_plus_half_buffer_height));

    text_renderer->text_prog.uniform4fv(
        "brightness_contrast", 2, auto_buffer_contrast_brightness);

    // Compute text box size
    float boxW = 0.0f;
    float boxH = 0.0f;
    for (const auto c : string{text}) {
        const auto uchar = static_cast<unsigned char>(c);
        boxW +=
            static_cast<float>(text_renderer->text_texture_advances[uchar][0]);
        boxH = max(
            boxH,
            static_cast<float>(text_renderer->text_texture_sizes[uchar][1]));
    }

    constexpr float paddingScale = 1.0f / (1.0f - 2.0f * padding);
    text_pixel_scale =
        max(text_pixel_scale, max(boxW, boxH) * paddingScale * channels);
    const float sx = 1.0f / text_pixel_scale;
    const float sy = 1.0f / text_pixel_scale;

    vec4 channel_offset(0.0f, y_offset, 0.0f, 0.0f);

    vec4 centeredCoord(x, y, 0.0f, 1.0f);

    if (static_cast<int>(buffer_component->buffer_width_f) % 2 == 0) {
        centeredCoord.x() += 0.5f;
    }
    if (static_cast<int>(buffer_component->buffer_height_f) % 2 == 0) {
        centeredCoord.y() += 0.5f;
    }

    centeredCoord = buffer_pose * centeredCoord;

    y = centeredCoord.y() + boxH / 2.0f * sy - channel_offset.y();
    x = centeredCoord.x() - boxW / 2.0f * sx - channel_offset.x();

    for (const auto c : string{text}) {
        const auto uchar = static_cast<unsigned char>(c);
        const float x2 =
            x +
            static_cast<float>(text_renderer->text_texture_tls[uchar][0]) * sx;
        const float y2 =
            y -
            static_cast<float>(text_renderer->text_texture_tls[uchar][1]) * sy;

        const int tex_wid = text_renderer->text_texture_sizes[uchar][0];
        const int tex_hei = text_renderer->text_texture_sizes[uchar][1];

        const float w = static_cast<float>(tex_wid) * sx;
        const float h = static_cast<float>(tex_hei) * sy;

        const float tex_lower_x =
            static_cast<float>(text_renderer->text_texture_offsets[uchar][0]) /
            text_renderer->text_texture_width;
        const float tex_lower_y =
            static_cast<float>(text_renderer->text_texture_offsets[uchar][1]) /
            text_renderer->text_texture_height;
        const float tex_upper_x =
            tex_lower_x + (static_cast<float>(tex_wid) - 1.0f) /
                              text_renderer->text_texture_width;
        const float tex_upper_y =
            tex_lower_y + (static_cast<float>(tex_hei) - 1.0f) /
                              text_renderer->text_texture_height;

        /*
         * box format: <pixel coord x, pixel coord y, texture coord x, texture
         * coord y>
         */
        const GLfloat box[4][4] = {
            {x2, y2, tex_lower_x, tex_lower_y},
            {x2 + w, y2, tex_upper_x, tex_lower_y},
            {x2, y2 + h, tex_lower_x, tex_upper_y},
            {x2 + w, y2 + h, tex_upper_x, tex_upper_y},
        };

        gl_canvas_->glBufferData(
            GL_ARRAY_BUFFER, sizeof box, box, GL_DYNAMIC_DRAW);
        gl_canvas_->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        vec4 char_step_direction(
            static_cast<float>(text_renderer->text_texture_advances[uchar][0]) *
                sx,
            static_cast<float>(text_renderer->text_texture_advances[uchar][1]) *
                sy,
            0.0f,
            1.0f);

        x += char_step_direction.x();
        y += char_step_direction.y();
    }
}

void BufferValues::shift_precision_left()
{
    if (min_float_precision < float_precision) {
        float_precision--;
        // reset text scaling
        text_pixel_scale = default_text_scale;
    }
}

void BufferValues::shift_precision_right()
{
    if (max_float_precision > float_precision) {
        float_precision++;
        // reset text scaling
        text_pixel_scale = default_text_scale;
    }
}

} // namespace oid
