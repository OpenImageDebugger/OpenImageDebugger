#include <GL/glew.h>

#include "buffer_values.hpp"
#include "stage.hpp"

using namespace std;

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

BufferValues::~BufferValues() {
    glDeleteTextures(1, &text_tex);
    glDeleteBuffers(1, &text_vbo);
}

bool BufferValues::initialize() {
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
                     ShaderProgram::FormatR, {
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

    game_object->set_render_index(50);

    return true;
}

void BufferValues::generate_glyphs_texture() {
    const char text[]="0123456789., -enaninf";
    const unsigned char *p;
    const int border_size = 2;

    FT_GlyphSlot g = font->glyph;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, text_tex);

    // Compute text box size
    float boxW = 0, boxH = 0;
    for(p = reinterpret_cast<const unsigned char*>(text); *p; p++) {
        if(FT_Load_Char(font, *p, FT_LOAD_RENDER))
            continue;
        text_texture_advances[*p][0] = (g->advance.x >> 6);
        text_texture_advances[*p][1] = (g->advance.y >> 6);
        text_texture_sizes[*p][0] = g->bitmap.width;
        text_texture_sizes[*p][1] = g->bitmap.rows;
        text_texture_tls[*p][0] = g->bitmap_left;
        text_texture_tls[*p][1] = g->bitmap_top;
        boxW += g->bitmap.width + 2*border_size;
        boxH = std::max(boxH, (float)g->bitmap.rows + 2*border_size);
    }

    text_texture_width = text_texture_height = 1.0f;
    while(text_texture_width<boxW) text_texture_width *= 2.f;
    while(text_texture_height<boxH) text_texture_height *= 2.f;

    const int mipmapLevels = 5;
    glTexStorage2D(GL_TEXTURE_2D, mipmapLevels, GL_R8, text_texture_width,
                   text_texture_height);

    // Clears generated buffer
    {
        std::vector<uint8_t> zeros(text_texture_width*text_texture_height, 0);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, text_texture_width,
                        text_texture_height, GL_RED, GL_UNSIGNED_BYTE, zeros.data());
    }

    int x = 0, y = 0;
    for(p = reinterpret_cast<const unsigned char*>(text); *p; p++) {
        if(FT_Load_Char(font, *p, FT_LOAD_RENDER))
            continue;

        glTexSubImage2D(GL_TEXTURE_2D, 0, x+border_size, y + border_size,
                        g->bitmap.width, g->bitmap.rows,
                        GL_RED, GL_UNSIGNED_BYTE, g->bitmap.buffer);
        text_texture_offsets[*p][0] = x + border_size;
        text_texture_offsets[*p][1] = y + border_size;

        x += g->bitmap.width + border_size*2;
        y += (g->advance.y >> 6);
    }

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
}

inline void pix2str(const Buffer::BufferType& type,
                    char* pix_label,
                    const uint8_t* buffer,
                    int& pos,
                    const int& c) {
    if(type == Buffer::BufferType::Float32 ||
       type == Buffer::BufferType::Float64) {
        float fpix = reinterpret_cast<const float*>(buffer)[pos + c];
        sprintf(pix_label, "%.3f", fpix);
        if(strlen(pix_label) > 7)
            sprintf(pix_label, "%.3e", fpix);
    }
    else if(type == Buffer::BufferType::UnsignedByte) {
        sprintf(pix_label, "%d", buffer[pos + c]);
    }
    else if(type == Buffer::BufferType::Short) {
        short fpix = reinterpret_cast<const short*>(buffer)[pos + c];
        sprintf(pix_label, "%d", fpix);
    }
    else if(type == Buffer::BufferType::UnsignedShort) {
        unsigned short fpix = reinterpret_cast<const unsigned short*>(buffer)[pos + c];
        sprintf(pix_label, "%d", fpix);
    }
    else if(type == Buffer::BufferType::Int32) {
        int fpix = reinterpret_cast<const int*>(buffer)[pos + c];
        sprintf(pix_label, "%d", fpix);
        if(strlen(pix_label) > 7)
            sprintf(pix_label, "%.3e", static_cast<float>(fpix));
    }
}

void BufferValues::draw(const mat4& projection, const mat4& viewInv) {
    GameObject* cam_obj = game_object->stage->getGameObject("camera");
    Camera* camera = cam_obj->getComponent<Camera>("camera_component");
    float zoom = camera->get_zoom();

    if(zoom > 40) {
        mat4 camRot = mat4::rotation(game_object->angle);

        Buffer* buffer_component = game_object->getComponent<Buffer>("buffer_component");
        float buffer_width_f = buffer_component->buffer_width_f;
        float buffer_height_f = buffer_component->buffer_height_f;
        int step = buffer_component->step;
        int channels = buffer_component->channels;
        Buffer::BufferType type = buffer_component->type;
        uint8_t* buffer = buffer_component->buffer;

        vec4 tl_ndc(-1,1,0,1);
        vec4 br_ndc(1,-1,0,1);
        mat4 vpInv = (projection*viewInv*camRot).inv();
        vec4 tl = vpInv*tl_ndc;
        vec4 br = vpInv*br_ndc;

        // Since the clip ROI may be rotated, we need to re-compute TL and BR from their Xs and Ys
        int tlX = min(tl.x(), br.x()), tlY = min(tl.y(), br.y());
        int brX = max(tl.x(), br.x()), brY = max(tl.y(), br.y());
        tl.x() = tlX, tl.y() = tlY;
        br.x() = brX, br.y() = brY;


        int lower_x = clamp(truncf(tl.x())-1.0f, -buffer_width_f/2.0f, buffer_width_f/2.0f-1.f);
        int upper_x = clamp(ceilf(br.x())+1.f, -(buffer_width_f+1)/2.0f+1.f, (buffer_width_f+1)/2.0f);

        int lower_y = clamp(truncf(tl.y())-1.0f, -buffer_height_f/2.0f, buffer_height_f/2.0f-1.0f);
        int upper_y = clamp(ceilf(br.y())+1.f, -(buffer_height_f+1)/2.0f+1.f, (buffer_height_f+1)/2.0f);

        int pos_center_x = -buffer_width_f/2;
        int pos_center_y = -buffer_height_f/2;

        char pix_label[30];

        int pos;
        float y_off;

        // Offset for vertical channel position to account for padding
        array<float, 4> recenterFactors;
        if(channels == 1) {
            recenterFactors = {0.f, 0.f,
                               0.f, 0.f};
        } else if(channels == 2) {
            float rfUp = padding / 3.0 / channels;
            recenterFactors = {rfUp,
                               -rfUp,
                               0.f, 0.f};
        } else if(channels == 3) {
            float rfUp = padding / 2.0 / channels;
            recenterFactors = {rfUp, 0.f,
                               -rfUp, 0.f};
        } else if(channels == 4) {
            float rfUp = 3.f * padding / 5.f / channels;
            float rfDown = padding / 5.f / channels;
            recenterFactors = {rfUp, rfDown,
                               -rfDown, -rfUp};
        }

        for(int y = lower_y-pos_center_y; y < upper_y-pos_center_y; ++y) {
            for(int x = lower_x-pos_center_x; x < upper_x-pos_center_x; ++x) {
                pos = (y*step + x)*channels;

                for(int c = 0; c < channels; ++c) {
                    y_off = (0.5f * (channels - 1) - c) / channels
                            - recenterFactors[c];

                    pix2str(type, pix_label, buffer, pos, c);
                    draw_text(projection, viewInv, camRot,
                              pix_label,
                              x + pos_center_x, y + pos_center_y,
                              y_off, channels);
                }
            }
        }
    }
}

void BufferValues::draw_text(const mat4& projection,
                             const mat4& viewInv,
                             const mat4& camRot,
                             const char* text,
                             float x,
                             float y,
                             float y_offset,
                             float channels) {
    Buffer* buffer_component = game_object->getComponent<Buffer>("buffer_component");
    const float* auto_buffer_contrast_brightness;
    if(game_object->stage->contrast_enabled) {
        auto_buffer_contrast_brightness =
                buffer_component->auto_buffer_contrast_brightness();
    } else {
        auto_buffer_contrast_brightness = Buffer::no_ac_params;
    }

    text_prog.use();
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);

    glActiveTexture(GL_TEXTURE0);
    GLuint buff_tex = buffer_component->sub_texture_id_at_coord(x + buffer_component->buffer_width_f/2.f,
                                                                y + buffer_component->buffer_height_f/2.f);
    glBindTexture(GL_TEXTURE_2D, buff_tex);
    text_prog.uniform1i("buff_sampler", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, text_tex);
    text_prog.uniform1i("text_sampler", 1);

    text_prog.uniformMatrix4fv("mvp", 1, GL_FALSE,
            (projection*viewInv).data());
    text_prog.uniform2f("pix_coord",
            buffer_component->tile_coord_x(x + buffer_component->buffer_width_f/2.f),
            buffer_component->tile_coord_y(y + buffer_component->buffer_height_f/2.f));

    text_prog.uniform4fv("brightness_contrast", 2,
            auto_buffer_contrast_brightness);

    // Compute text box size
    float boxW = 0, boxH = 0;
    for(const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; p++) {
        boxW += text_texture_advances[*p][0];
        boxH = max(boxH, (float)text_texture_sizes[*p][1]);
    }

    float paddingScale = 1.f/(1.f-2.f*padding);
    text_pixel_scale = max(text_pixel_scale,
                           max(boxW, boxH) * paddingScale * channels);
    float sx = 1.0/text_pixel_scale;
    float sy = 1.0/text_pixel_scale;

    vec4 channel_offset(0, y_offset, 0, 0);

    vec4 centeredCoord(x, y, 0, 1);

    if(static_cast<int>(buffer_component->buffer_width_f)%2 == 0) {
        centeredCoord.x() += 0.5f;
    }
    if(static_cast<int>(buffer_component->buffer_height_f)%2 == 0) {
        centeredCoord.y() += 0.5f;
    }

    centeredCoord = camRot * centeredCoord;

    y = centeredCoord.y() + boxH/2.0*sy - channel_offset.y();
    x = centeredCoord.x() - boxW/2.0*sx - channel_offset.x();

    for(const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; p++) {
        float x2 = x + text_texture_tls[*p][0] * sx;
        float y2 = y - text_texture_tls[*p][1] * sy;

        int tex_wid = text_texture_sizes[*p][0];
        int tex_hei = text_texture_sizes[*p][1];
        float w = tex_wid * sx;
        float h = tex_hei * sy;

        float tex_lower_x = ((float)text_texture_offsets[*p][0])/text_texture_width;
        float tex_lower_y = ((float)text_texture_offsets[*p][1])/text_texture_height;
        float tex_upper_x = tex_lower_x + ((float)tex_wid-1.0f)/text_texture_width;
        float tex_upper_y = tex_lower_y + ((float)tex_hei-1.0f)/text_texture_height;

        /*
         * box format: <pixel coord x, pixel coord y, texture coord x, texture coord y>
         */
        GLfloat box[4][4] = {
            {x2,     y2    , tex_lower_x, tex_lower_y},
            {x2 + w, y2    , tex_upper_x, tex_lower_y},
            {x2,     y2 + h, tex_lower_x, tex_upper_y},
            {x2 + w, y2 + h, tex_upper_x, tex_upper_y},
        };

        glBufferData(GL_ARRAY_BUFFER, sizeof box, box, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        vec4 char_step_direction(text_texture_advances[*p][0] * sx, text_texture_advances[*p][1] * sy, 0.0, 1.0);

        x += char_step_direction.x();
        y += char_step_direction.y();
    }
}
