#include "math.hpp"

#include <iostream>
using namespace Eigen;
using namespace std;

vec4::vec4(){}


void vec4::operator=(const vec4 &b) {
    vec = b.vec;
}

vec4& vec4::operator+=(const vec4 &b)
{
    for(int i = 0; i < 4; ++i)
        vec[i]+=b.vec[i];
    return *this;
}

vec4 vec4::operator-(const vec4 &b)
{
    return vec4(vec[0]-b.vec[0], vec[1]-b.vec[1], vec[2]-b.vec[2], vec[3]-b.vec[3]);
}

vec4::vec4(float x, float y, float z, float w) :
    vec(x, y, z, w) {
}

void vec4::print() const {
    std::cout << vec << std::endl;
}

float *vec4::data() {
    return vec.data();
}

float &vec4::x() {
    return vec[0];
}

float &vec4::y() {
    return vec[1];
}

float &vec4::z() {
    return vec[2];
}

float &vec4::w() {
    return vec[3];
}

vec4 vec4::zero() {
    return vec4(0,0,0,0);
}


void mat4::setIdentity() {
    const float I[] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    memcpy(mat.data(), I, sizeof(I));
}

void mat4::setFromST(float scaleX, float scaleY, float scaleZ, float x, float y, float z) {
    float* data = this->data();

    data[0] = scaleX, data[5] = scaleY, data[10] = scaleZ;
    data[12]=x, data[13]=y, data[14]=z;

    data[1] = data[2] = data[3] = data[4] = 0.0;
    data[6] = data[7] = data[8] = data[9] = 0.0;
    data[11] = 0.0;
    data[15] = 1.0;
}

void mat4::setFromSRT(float scaleX, float scaleY, float scaleZ, float rZ, float x, float y, float z) {
    Affine3f t = Affine3f::Identity();
    t.translate(Vector3f(x,y,z)).rotate(AngleAxisf(rZ, Vector3f(0,0,1))).scale(Vector3f(scaleX,scaleY,scaleZ));
    this->mat = t.matrix();

    /*
    float* data = this->data();

    data[0] = scaleX, data[5] = scaleY, data[10] = scaleZ;
    data[12]=x, data[13]=y, data[14]=z;

    data[1] = data[2] = data[3] = data[4] = 0.0;
    data[6] = data[7] = data[8] = data[9] = 0.0;
    data[11] = 0.0;
    data[15] = 1.0;
    */
}

float *mat4::data() {
    return mat.data();
}

mat4 mat4::rotation(float angle) {
    mat4 result;
    Affine3f t = Affine3f::Identity();
    t.rotate(AngleAxisf(angle, Vector3f(0,0,1)));

    result.mat = t.matrix();
    return result;
}

void mat4::setOrthoProjection(float right, float top, float near, float far) {
    float* data = this->data();

    data[0] = 1.0/right;
    data[5] = -1.0/top;
    data[10] = -2.0/(far-near);
    data[14] = -(far+near)/(far-near);

    data[1] = data[2] = data[3] = data[4] = 0.0;
    data[6] = data[7] = data[8] = data[9] = 0.0;
    data[11] = data[12] = data[13] = 0.0;
    data[15] = 1.0;
}

void mat4::print() const {
    std::cout << mat << std::endl;
}

mat4 mat4::inv() const {
    mat4 res;
    res.mat = this->mat.inverse();

    return res;
}

vec4 mat4::operator*(const vec4 &b) const {
    vec4 res;
    res.vec = this->mat * b.vec;

    return res;
}

mat4 mat4::operator*(const mat4 &b) const {
    mat4 res;

    res.mat = this->mat * b.mat;

    return res;
}
