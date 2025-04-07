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

#include "vec4.h"

#include <iostream>

namespace oid
{

vec4::vec4(const float x, const float y, const float z, const float w)
    : vec_{x, y, z, w}
{
}


vec4& vec4::operator+=(const vec4& b)
{
    for (int i = 0; i < 4; ++i) {
        vec_[i] += b.vec_[i];
    }
    return *this;
}


void vec4::print() const
{
    std::cout << vec_.transpose() << std::endl;
}


float* vec4::data()
{
    return vec_.data();
}


float& vec4::x()
{
    return vec_[0];
}


float& vec4::y()
{
    return vec_[1];
}


float& vec4::z()
{
    return vec_[2];
}


float& vec4::w()
{
    return vec_[3];
}


const float& vec4::x() const
{
    return vec_[0];
}


const float& vec4::y() const
{
    return vec_[1];
}


const float& vec4::z() const
{
    return vec_[2];
}


const float& vec4::w() const
{
    return vec_[3];
}


vec4 vec4::zero()
{
    return {0.0f, 0.0f, 0.0f, 0.0f};
}


vec4 operator-(const vec4& vector)
{
    return {-vector.x(), -vector.y(), -vector.z(), -vector.w()};
}

} // namespace oid
