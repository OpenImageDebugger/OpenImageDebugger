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

#ifndef LINEAR_ALGEBRA_H_
#define LINEAR_ALGEBRA_H_

#include <Eigen>

namespace oid
{

class mat4;

class vec4
{
    friend class mat4;

  public:
    vec4();

    ~vec4() = default;

    vec4(const vec4& b);

    vec4(vec4&& b) = default;

    vec4& operator=(const vec4& b);

    vec4& operator=(vec4&& b) = default;

    vec4& operator+=(const vec4& b);

    vec4 operator+(const vec4& b) const;

    vec4 operator-(const vec4& b) const;

    vec4 operator*(float scalar) const;

    vec4(float x, float y, float z, float w);

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
    Eigen::Vector4f vec_;
};

vec4 operator-(const vec4& vector);


class mat4
{
  public:
    void set_identity();

    void set_from_srt(float scaleX,
                      float scaleY,
                      float scaleZ,
                      float rZ,
                      float x,
                      float y,
                      float z);

    void set_from_st(float scaleX,
                     float scaleY,
                     float scaleZ,
                     float x,
                     float y,
                     float z);

    float* data();

    void operator<<(const std::initializer_list<float>& data);

    void set_ortho_projection(float right, float top, float near, float far);

    void print() const;

    [[nodiscard]] mat4 inv() const;

    mat4 operator*(const mat4& b) const;

    vec4 operator*(const vec4& b) const;

    float& operator()(int row, int col);

    static mat4 rotation(float angle);

    static mat4 translation(const vec4& vector);

    static mat4 scale(const vec4& factor);

  private:
    Eigen::Matrix4f mat_;
};

} // namespace oid

#endif // LINEAR_ALGEBRA_H_
