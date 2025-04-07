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

#ifndef MATH_VEC4_H_
#define MATH_VEC4_H_

#include <Eigen>

namespace oid
{

class vec4
{
    friend class mat4;

  public:
    vec4() = default;

    vec4(float x, float y, float z, float w);

    vec4& operator+=(const vec4& b);

    friend vec4 operator+(const vec4& a, const vec4& b)
    {
        return {a.vec_[0] + b.vec_[0],
                a.vec_[1] + b.vec_[1],
                a.vec_[2] + b.vec_[2],
                a.vec_[3] + b.vec_[3]};
    }

    friend vec4 operator-(const vec4& a, const vec4& b)
    {
        return {a.vec_[0] - b.vec_[0],
                a.vec_[1] - b.vec_[1],
                a.vec_[2] - b.vec_[2],
                a.vec_[3] - b.vec_[3]};
    }

    friend vec4 operator*(const vec4& vec, const float scalar)
    {
        auto result = vec4{vec};
        result.vec_ *= scalar;

        return result;
    }

    void print() const;

    float* data();

    float& x();
    float& y();
    float& z();
    float& w();

    [[nodiscard]] const float& x() const;
    [[nodiscard]] const float& y() const;
    [[nodiscard]] const float& z() const;
    [[nodiscard]] const float& w() const;

    static vec4 zero();

  private:
    Eigen::Vector4f vec_{};
};

vec4 operator-(const vec4& vector);

} // namespace oid

#endif // MATH_VEC4_H_
