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
    if((enable_borders==1) && (abs(err.x)<0.01||abs(err.y)<0.01)) {
        color.rgb = vec3(1.0);
    }
    else {
        color.rgb = color.rgb*brightness_contrast[0] + brightness_contrast[1];
    }
}

)";

} // namespace shader
