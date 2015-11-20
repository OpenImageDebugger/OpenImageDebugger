#include <GL/glew.h>

#include "buffer.hpp"
#include "stage.hpp"

using namespace std;

Buffer::~Buffer() {
    int num_textures = num_textures_x*num_textures_y;
    glDeleteTextures(num_textures, buff_tex.data());
    glDeleteBuffers(1, &vbo);
}

void Buffer::update() {
    Camera* camera = stage->getComponent<Camera>("camera_component");
    float zoom = camera->zoom;
    if(zoom > 40) {
        buff_prog.uniform1i("enable_borders", 1);
    } else {
        buff_prog.uniform1i("enable_borders", 0);
    }
}

bool Buffer::initialize() {
    // Buffer Shaders
    ShaderProgram::TexelChannels channelType;
    if(channels == 1)
        channelType = ShaderProgram::Grayscale;
    else
        channelType = ShaderProgram::RGB;

    buff_prog.create(shader::buff_vert_shader,
                     shader::buff_frag_shader,
                     channelType, { "mvp",
                                    "sampler", "brightness_contrast",
                                    "buffer_dimension", "enable_borders"});

    // Buffer VBO
    static const GLfloat g_vertex_buffer_data[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f,  1.0f,
        1.0f,  1.0f,
        0.0f,  1.0f,
        0.0f, 0.0f,
    };

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

    setup_gl_buffer();

    return true;
}


void Buffer::draw(const mat4& projection, const mat4& viewInv) {
    buff_prog.use();
    glEnableVertexAttribArray(0);
    glActiveTexture(GL_TEXTURE0);
    buff_prog.uniform1i("sampler", 0);
    buff_prog.uniform3fv("brightness_contrast", 2, auto_buffer_contrast_brightness);

    int buffer_width_i = static_cast<int>(buffer_width_f);
    int buffer_height_i = static_cast<int>(buffer_height_f);

    int remaining_h = buffer_height_i;
    for(int ty = 0; ty < num_textures_y; ++ty) {
        int buff_h = std::min(remaining_h, max_texture_size);
        remaining_h -= buff_h;

        int remaining_w = buffer_width_i;
        for(int tx = 0; tx < num_textures_x; ++tx) {
            int buff_w = std::min(remaining_w, max_texture_size);
            remaining_w -= buff_w;

            glBindTexture(GL_TEXTURE_2D, buff_tex[ty*num_textures_x+tx]);

            mat4 tile_model;
            tile_model.setFromST(buff_w, buff_h, 1.0,
                                 posX()+tx*max_texture_size,
                                 posY()+ty*max_texture_size, 0.0f);
            buff_prog.uniformMatrix4fv("mvp", 1, GL_FALSE, (projection * viewInv * tile_model).data);
            buff_prog.uniform2f("buffer_dimension", buff_w, buff_h);

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }
}

void Buffer::setup_gl_buffer() {
    int buffer_width_i = static_cast<int>(buffer_width_f);
    int buffer_height_i = static_cast<int>(buffer_height_f);

    model.setFromST(buffer_width_i, buffer_height_i, 1.0, 0.0, 0.0, 0.0f);

    // Initialize contrast parameters
    computeContrastBrightnessParameters();

    // Buffer texture
    num_textures_x = ceil(((float)buffer_width_i)/((float)max_texture_size));
    num_textures_y = ceil(((float)buffer_height_i)/((float)max_texture_size));
    int num_textures = num_textures_x*num_textures_y;

    buff_tex.resize(num_textures);
    glGenTextures(num_textures, buff_tex.data());

    GLuint tex_type = GL_UNSIGNED_BYTE;
    GLuint tex_format = GL_RED;
    if(type == 0) {
        tex_type = GL_FLOAT;
    } else if (type == 1) {
        tex_type = GL_UNSIGNED_BYTE;
    }
    if(channels == 1) {
        tex_format = GL_RED;
    } else if(channels == 3) {
        tex_format = GL_RGB;
    }

    int remaining_h = buffer_height_i;
    glPixelStoref(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, step);

    for(int ty = 0; ty < num_textures_y; ++ty) {
        int buff_h = std::min(remaining_h, max_texture_size);
        remaining_h -= buff_h;

        int remaining_w = buffer_width_i;
        for(int tx = 0; tx < num_textures_x; ++tx) {
            int buff_w = std::min(remaining_w, max_texture_size);
            remaining_w -= buff_w;

            int tex_id = ty*num_textures_x + tx;
            glBindTexture(GL_TEXTURE_2D, buff_tex[tex_id]);

            glPixelStorei(GL_UNPACK_SKIP_ROWS, ty*max_texture_size);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, tx*max_texture_size);
            glTexStorage2D( GL_TEXTURE_2D, 1, GL_RGB32F, buff_w, buff_h);

            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                            buff_w, buff_h, tex_format, tex_type,
                            reinterpret_cast<GLvoid*>(buffer));

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
}
