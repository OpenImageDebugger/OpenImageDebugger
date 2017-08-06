namespace shader {

const char* buff_frag_shader = R"(

uniform sampler2D sampler;
uniform vec4 brightness_contrast[2];
uniform vec2 buffer_dimension;
uniform int enable_borders;

// Ouput data
varying vec2 uv;

void main()
{
    vec4 color;
#if defined(FORMAT_R)
    // Output color = grayscale
    color = texture2D(sampler, uv).rrra;
    color.rgb = color.rgb * brightness_contrast[0].xxx + brightness_contrast[1].xxx;
#elif defined(FORMAT_RG)
    // Output color = two channels
    color = texture2D(sampler, uv);
    color.rg = color.rg * brightness_contrast[0].xy + brightness_contrast[1].xy;
    color.b = 0.0;
#elif defined(FORMAT_RGB)
    // Output color = rgb
    color = texture2D(sampler, uv);
    color.rgb = color.rgb * brightness_contrast[0].xyz + brightness_contrast[1].xyz;
#else
    // Output color = rgba
    color = texture2D(sampler, uv);
    color = color * brightness_contrast[0] + brightness_contrast[1];
#endif

    vec2 buffer_position = uv*buffer_dimension;

    if(enable_borders == 1) {
        float alpha = max(abs(dFdx(buffer_position.x)),
                          abs(dFdx(buffer_position.y)));

        float x_ = fract(buffer_position.x);
        float y_ = fract(buffer_position.y);

        float vertical_border = clamp(abs(-1.0/alpha * x_ + 0.5/alpha) -
                                      (0.5/alpha-1.0), 0.0, 1.0);

        float horizontal_border = clamp(abs(-1.0/alpha * y_ + 0.5/alpha) -
                                        (0.5/alpha-1.0), 0.0, 1.0);

        color.rgb += vec3(vertical_border +
                          horizontal_border);
    }

    gl_FragColor = color.PIXEL_LAYOUT;
}

)";

} // namespace shader
