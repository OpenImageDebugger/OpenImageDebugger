namespace shader {

const char* buff_frag_shader = R"(

uniform sampler2D sampler;
uniform vec3 brightness_contrast[2];
uniform vec2 buffer_dimension;
uniform int enable_borders;

// Ouput data
in vec2 uv;
out vec4 color;

void main()
{
	// Output color = red 
#ifdef GRAYSCALE
    color = texture(sampler, uv).rrra;
#else
    color = texture(sampler, uv);
#endif

    vec2 buffer_position = uv*buffer_dimension;
    vec2 err = round(buffer_position)-buffer_position;
    if(enable_borders==1) {
        color.rgb = color.rgb*brightness_contrast[0] + brightness_contrast[1];

        const float alpha = 0.01;
        float x_ = fract(buffer_position.x);
        float y_ = fract(buffer_position.y);
        float vertical_border = clamp(abs(-1.0/alpha * x_ + 0.5/alpha) - (0.5/alpha-1.0), 0.0, 1.0);
        float horizontal_border = clamp(abs(-1.0/alpha * y_ + 0.5/alpha) - (0.5/alpha-1.0), 0.0, 1.0);
        color.rgb += vec3(vertical_border+horizontal_border);
    }
    else {
        color.rgb = color.rgb*brightness_contrast[0] + brightness_contrast[1];
    }
}

)";

} // namespace shader
