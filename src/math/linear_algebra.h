#pragma once

#include <cstring>
#include <iostream>

#include <eigen3/Eigen/Eigen>

class mat4;

class vec4 {
    friend class mat4;
public:
    vec4();
    void operator=(const vec4& b);

    vec4& operator+=(const vec4& b);
    vec4 operator-(const vec4& b);

    vec4(float x, float y, float z, float w);

    void print() const;

    float* data();

    float& x();
    float& y();
    float& z();
    float& w();

    static vec4 zero();

private:
    Eigen::Vector4f vec;
};

class mat4 {
public:
    void setIdentity();

    void setFromSRT(float scaleX, float scaleY, float scaleZ, float rZ, float x, float y, float z);

    void setFromST(float scaleX, float scaleY, float scaleZ, float x, float y, float z);

    float* data();

    void setOrthoProjection(float right, float top, float near, float far);

    void print() const;

    mat4 inv() const;

    mat4 operator*(const mat4& b) const;

    vec4 operator*(const vec4& b) const;

    static mat4 rotation(float angle);
private:
    Eigen::Matrix4f mat;
};

