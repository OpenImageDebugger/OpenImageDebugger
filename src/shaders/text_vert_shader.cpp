namespace shader {

const char* text_vert_shader = R"(

in vec4 inputPosition;
out vec2 uv;

uniform mat4 mvp;

void main(void) {
  gl_Position = mvp*vec4(inputPosition.xy, 0.0, 1.0);
  uv = inputPosition.zw;
}

)";

} // namespace shader

