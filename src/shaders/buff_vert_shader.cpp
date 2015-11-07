namespace shader {

const char* buff_vert_shader = R"(

in vec2 inputPosition;
out vec2 uv;

uniform mat4 mvp;

void main(void) {
    uv = inputPosition;
    gl_Position = mvp*vec4(inputPosition,0.0,1.0);
}

)";

} // namespace shader
