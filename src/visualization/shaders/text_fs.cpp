/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2024 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

namespace shader
{

const char* text_frag_shader = R"(

uniform sampler2D buff_sampler;
uniform sampler2D text_sampler;
uniform vec2 pix_coord;
uniform vec4 brightness_contrast[2];


// Ouput data
varying vec2 uv;


float round_float(float f) {
    return float(int(f + 0.5));
}


bool oid_isnan(float val) {
    return (val < 0.0 || 0.0 < val || val == 0.0) ? false : true;
}


void main()
{
    vec4 color;
    // Output color = red
    float buff_color = texture2D(buff_sampler, pix_coord).r;
    buff_color = buff_color * brightness_contrast[0].x +
                              brightness_contrast[1].x;

    if (oid_isnan(buff_color)) {
        buff_color = 0.0;
    }

    float text_color = texture2D(text_sampler, uv).r;
    float pix_intensity = round_float(1.0 - buff_color);

    color = vec4(vec3(pix_intensity), text_color);

    gl_FragColor = color;
}

)";

} // namespace shader
