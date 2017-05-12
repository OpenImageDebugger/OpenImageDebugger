namespace shader {

const char* background_vert_shader = R"(

attribute vec2 inputPosition;

void main(void) {
    gl_Position = vec4(inputPosition,
                           0.0,1.0);
}

)";

} // namespace shader
