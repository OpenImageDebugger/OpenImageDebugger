/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 GDB ImageWatch contributors
 * (github.com/csantosbh/gdb-imagewatch/)
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

#include <iostream>

#include "linear_algebra.h"


vec4::vec4()
{
}


vec4::vec4(float x, float y, float z, float w)
    : vec(x, y, z, w)
{
}


void vec4::operator=(const vec4& b)
{
    vec = b.vec;
}


vec4& vec4::operator+=(const vec4& b)
{
    for (int i = 0; i < 4; ++i)
        vec[i] += b.vec[i];
    return *this;
}


vec4 vec4::operator+(const vec4& b) const
{
    return vec4(vec[0] + b.vec[0],
                vec[1] + b.vec[1],
                vec[2] + b.vec[2],
                vec[3] + b.vec[3]);
}


vec4 vec4::operator-(const vec4& b) const
{
    return vec4(vec[0] - b.vec[0],
                vec[1] - b.vec[1],
                vec[2] - b.vec[2],
                vec[3] - b.vec[3]);
}


vec4 vec4::operator*(float scalar) const
{
    vec4 result(*this);
    result.vec *= scalar;

    return result;
}


void vec4::print() const
{
    std::cout << vec.transpose() << std::endl;
}


float* vec4::data()
{
    return vec.data();
}


float& vec4::x()
{
    return vec[0];
}


float& vec4::y()
{
    return vec[1];
}


float& vec4::z()
{
    return vec[2];
}


float& vec4::w()
{
    return vec[3];
}


const float& vec4::x() const
{
    return vec[0];
}


const float& vec4::y() const
{
    return vec[1];
}


const float& vec4::z() const
{
    return vec[2];
}


const float& vec4::w() const
{
    return vec[3];
}


vec4 vec4::zero()
{
    return vec4(0, 0, 0, 0);
}


vec4 operator-(const vec4& vector)
{
    return {-vector.x(), -vector.y(), -vector.z(), -vector.w()};
}


void mat4::set_identity()
{
    // clang-format off
    *this << std::initializer_list<float>{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    // clang-format on
}


void mat4::set_from_st(float scaleX,
                       float scaleY,
                       float scaleZ,
                       float x,
                       float y,
                       float z)
{
    float* data = this->data();

    data[0] = scaleX, data[5] = scaleY, data[10] = scaleZ;
    data[12] = x, data[13] = y, data[14] = z;

    data[1] = data[2] = data[3] = data[4] = 0.0;
    data[6] = data[7] = data[8] = data[9] = 0.0;
    data[11]                              = 0.0;
    data[15]                              = 1.0;
}


void mat4::set_from_srt(float scaleX,
                        float scaleY,
                        float scaleZ,
                        float rZ,
                        float x,
                        float y,
                        float z)
{
    using Eigen::Affine3f;
    using Eigen::AngleAxisf;
    using Eigen::Vector3f;

    Affine3f t = Affine3f::Identity();
    t.translate(Vector3f(x, y, z))
        .rotate(AngleAxisf(rZ, Vector3f(0, 0, 1)))
        .scale(Vector3f(scaleX, scaleY, scaleZ));
    this->mat_ = t.matrix();
}


float* mat4::data()
{
    return mat_.data();
}


void mat4::operator<<(const std::initializer_list<float>& data)
{
    memcpy(mat_.data(), data.begin(), sizeof(float) * data.size());
}


mat4 mat4::rotation(float angle)
{
    using Eigen::Affine3f;
    using Eigen::AngleAxisf;
    using Eigen::Vector3f;

    mat4 result;
    Affine3f t = Affine3f::Identity();
    t.rotate(AngleAxisf(angle, Vector3f(0, 0, 1)));

    result.mat_ = t.matrix();
    return result;
}


mat4 mat4::translation(const vec4& vector)
{
    using Eigen::Affine3f;
    using Eigen::Vector3f;

    mat4 result;

    Affine3f t = Affine3f::Identity();
    t.translate(Vector3f(vector.x(), vector.y(), vector.z()));
    result.mat_ = t.matrix();

    return result;
}


mat4 mat4::scale(const vec4& factor)
{
    using Eigen::Affine3f;
    using Eigen::Vector3f;

    mat4 result;

    Affine3f t = Affine3f::Identity();
    t.scale(Vector3f(factor.x(), factor.y(), factor.z()));
    result.mat_ = t.matrix();

    return result;
}


void mat4::set_ortho_projection(float right, float top, float near, float far)
{
    float* data = this->data();

    data[0]  = 1.0 / right;
    data[5]  = -1.0 / top;
    data[10] = -2.0 / (far - near);
    data[14] = -(far + near) / (far - near);

    data[1] = data[2] = data[3] = data[4] = 0.0;
    data[6] = data[7] = data[8] = data[9] = 0.0;
    data[11] = data[12] = data[13] = 0.0;
    data[15]                       = 1.0;
}


void mat4::print() const
{
    std::cout << mat_ << std::endl;
}


mat4 mat4::inv() const
{
    mat4 res;
    res.mat_ = this->mat_.inverse();

    return res;
}


vec4 mat4::operator*(const vec4& b) const
{
    vec4 res;
    res.vec = this->mat_ * b.vec;

    return res;
}


float&mat4::operator()(int row, int col) {
    return mat_(row, col);
}


mat4 mat4::operator*(const mat4& b) const
{
    mat4 res;

    res.mat_ = this->mat_ * b.mat_;

    return res;
}
