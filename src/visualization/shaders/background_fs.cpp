namespace shader {

const char* background_frag_shader = R"(

void main()
{
    const int tileSize = 10;
    float intensity = mod(floor(gl_FragCoord.x / tileSize) +
                          floor(gl_FragCoord.y / tileSize), 2);
    intensity = intensity * 0.2 + 0.4;
    gl_FragColor = vec4(vec3(intensity),1);
}

)";

} // namespace shader
