namespace shader {

const char* text_frag_shader = R"(

uniform sampler2D buff_sampler;
uniform sampler2D text_sampler;
uniform vec2 pix_coord;
uniform vec4 brightness_contrast[2];

// Ouput data
varying vec2 uv;

void main()
{
    vec4 color;
    // Output color = red
    float buff_color = texture2D(buff_sampler, pix_coord).r;
    buff_color = buff_color*brightness_contrast[0].x + brightness_contrast[1].x;

    float text_color = texture2D(text_sampler, uv).r;
    float pix_intensity = round(1.0-buff_color);

    color = vec4(vec3(pix_intensity), text_color);

    gl_FragColor = color;
}

)";

} // namespace shader
