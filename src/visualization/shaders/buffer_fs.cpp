/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2025 OpenImageDebugger contributors
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

namespace oid::shader
{
extern auto const buff_frag_shader{R"(

uniform sampler2D sampler;
uniform vec4 brightness_contrast[2];
uniform vec2 buffer_dimension;
uniform int enable_borders;
uniform int enable_icon_mode;

// Ouput data
varying vec2 uv;

void main()
{
    vec4 color;

#if defined(FORMAT_R)
    // Output color = grayscale
    color = texture2D(sampler, uv).rrra;
    color.rgb = color.rgb * brightness_contrast[0].xxx +
                            brightness_contrast[1].xxx;
#elif defined(FORMAT_RG)
    // Output color = two channels
    color = texture2D(sampler, uv);
    color.rg = color.rg * brightness_contrast[0].xy +
                          brightness_contrast[1].xy;
    color.b = 0.0;
#elif defined(FORMAT_RGB)
    // Output color = rgb
    color = texture2D(sampler, uv);
    color.rgb = color.rgb * brightness_contrast[0].xyz +
                            brightness_contrast[1].xyz;
#else
    // Output color = rgba
    color = texture2D(sampler, uv);
    color = color * brightness_contrast[0] +
                    brightness_contrast[1];
#endif

    vec2 buffer_position = uv * buffer_dimension;

    if(enable_icon_mode == 0 && enable_borders != 0) {
        float alpha = max(abs(dFdx(buffer_position.x)),
                          abs(dFdx(buffer_position.y)));

        float x_ = fract(buffer_position.x);
        float y_ = fract(buffer_position.y);

        float vertical_border = clamp(abs(-1.0 / alpha * x_ + 0.5 / alpha) -
                                      (0.5 / alpha - 1.0), 0.0, 1.0);

        float horizontal_border = clamp(abs(-1.0 / alpha * y_ + 0.5 / alpha) -
                                           (0.5 / alpha - 1.0), 0.0, 1.0);

        float ratio_a = max(vertical_border, horizontal_border);
        float ratio_b = 1.0 - ratio_a;

        color.r = color.r * ratio_b + 0.5 * ratio_a;
        color.g = color.g * ratio_b + 0.5 * ratio_a;
        color.b = color.b * ratio_b + 0.5 * ratio_a;
    }

    gl_FragColor = color.PIXEL_LAYOUT;
}

)"};
} // namespace oid::shader
