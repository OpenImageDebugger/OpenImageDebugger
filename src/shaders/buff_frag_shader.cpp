namespace shader {

const char* buff_frag_shader = R"(

uniform sampler2D sampler;
uniform vec4 brightness_contrast[2];
uniform vec2 buffer_dimension;
uniform int enable_borders;

// Ouput data
in vec2 uv;
out vec4 color;

void main()
{
#if defined(FORMAT_R)
    // Output color = grayscale
    color = texture(sampler, uv).rrra;
    color.r = color.r * brightness_contrast[0].x + brightness_contrast[1].x;
#elif defined(FORMAT_RG)
    // Output color = two channels
    color = texture(sampler, uv);
    color.rg = color.rg * brightness_contrast[0].xy + brightness_contrast[1].xy;
    color.b = 0.0;
#elif defined(FORMAT_RGB)
    // Output color = rgb
    color = texture(sampler, uv);
    color.rgb = color.rgb * brightness_contrast[0].xyz + brightness_contrast[1].xyz;
#else
    // Output color = rgba
    color = texture(sampler, uv);
    color = color * brightness_contrast[0] + brightness_contrast[1];
#endif

    vec2 buffer_position = uv*buffer_dimension;
    vec2 err = round(buffer_position)-buffer_position;

    if(enable_borders==1) {
        const float alpha = 0.01;
        float x_ = fract(buffer_position.x);
        float y_ = fract(buffer_position.y);
        float vertical_border = clamp(abs(-1.0/alpha * x_ + 0.5/alpha) - (0.5/alpha-1.0), 0.0, 1.0);
        float horizontal_border = clamp(abs(-1.0/alpha * y_ + 0.5/alpha) - (0.5/alpha-1.0), 0.0, 1.0);
        color.rgb += vec3(vertical_border+horizontal_border);
    }
}

)";

} // namespace shader
