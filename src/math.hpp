#pragma once

#include <cstring>
#include <iostream>
#include <algorithm> // std::max,min

template<typename T>
int clamp(T value, T lower, T upper) {
    return std::min(std::max(value, lower), upper);
}

class vec4 {
public:
    vec4(){}
    vec4(float x, float y, float z, float w) :
    x(x), y(y), z(z), w(w) {
    }

    void print() {
        for(int i = 0; i < 4; ++i)
            std::cout << data[i] << " ";
        std::cout << std::endl;
    }

    union {
        float data[4];
        struct {
            float x;
            float y;
            float z;
            float w;
        };
    };
};
class mat4 {
public:
    mat4() {
    }

    void setIdentity() {
        const float I[] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        memcpy(data, I, sizeof(I));
    }

    void setFromST(float scaleX, float scaleY, float scaleZ, float x, float y, float z) {
        data[0] = scaleX, data[5] = scaleY, data[10] = scaleZ;
        data[12]=x, data[13]=y, data[14]=z;

        data[1] = data[2] = data[3] = data[4] = 0.0;
        data[6] = data[7] = data[8] = data[9] = 0.0;
        data[11] = 0.0;
        data[15] = 1.0;
    }

    void setOrthoProjection(float right, float top, float near, float far) {
        data[0] = 1.0/right;
        data[5] = 1.0/top;
        data[10] = -2.0/(far-near);
        data[14] = -(far+near)/(far-near);

        data[1] = data[2] = data[3] = data[4] = 0.0;
        data[6] = data[7] = data[8] = data[9] = 0.0;
        data[11] = data[12] = data[13] = 0.0;
        data[15] = 1.0;
    }

    void print() {
        for(int y = 0; y < 4; ++y) {
            for(int x = 0; x < 4; ++x)
                printf("%.4e\t ",data[4*x+y]);
            std::cout << std::endl;
        }
    }

    // WARNING: This inversion method only works for scale/translation
    // matrices!
    mat4 inv() const {
        mat4 res;
        const float* a = this->data;
        float* r = res.data;

        r[0] = 1.0/a[0], r[5] = 1.0/a[5], r[10] = 1.0/a[10];
        r[12] = -r[0]*a[12], r[13] = -r[5]*a[13], r[14] = -r[10]*a[14];
        r[1] = r[2] = r[3] = r[4] = 0.0;
        r[6] = r[7] = r[8] = r[9] = 0.0;
        r[11] = 0.0;
        r[15] = 1.0;

        return res;
    }

    mat4 operator*(const mat4& b) const {
        const float* a = this->data;
        mat4 res;
        // First col
        res.data[0]  = a[0]*b.col0[0] + a[4]*b.col0[1] + a[8]*b.col0[2]  + a[12]*b.col0[3];
        res.data[1]  = a[1]*b.col0[0] + a[5]*b.col0[1] + a[9]*b.col0[2]  + a[13]*b.col0[3];
        res.data[2]  = a[2]*b.col0[0] + a[6]*b.col0[1] + a[10]*b.col0[2] + a[14]*b.col0[3];
        res.data[3]  = a[3]*b.col0[0] + a[7]*b.col0[1] + a[11]*b.col0[2] + a[15]*b.col0[3];

        // Second col
        res.data[4]  = a[0]*b.col1[0] + a[4]*b.col1[1] + a[8]*b.col1[2]  + a[12]*b.col1[3];
        res.data[5]  = a[1]*b.col1[0] + a[5]*b.col1[1] + a[9]*b.col1[2]  + a[13]*b.col1[3];
        res.data[6]  = a[2]*b.col1[0] + a[6]*b.col1[1] + a[10]*b.col1[2] + a[14]*b.col1[3];
        res.data[7]  = a[3]*b.col1[0] + a[7]*b.col1[1] + a[11]*b.col1[2] + a[15]*b.col1[3];

        // Third col
        res.data[8]  = a[0]*b.col2[0] + a[4]*b.col2[1] + a[8]*b.col2[2]  + a[12]*b.col2[3];
        res.data[9]  = a[1]*b.col2[0] + a[5]*b.col2[1] + a[9]*b.col2[2]  + a[13]*b.col2[3];
        res.data[10] = a[2]*b.col2[0] + a[6]*b.col2[1] + a[10]*b.col2[2] + a[14]*b.col2[3];
        res.data[11] = a[3]*b.col2[0] + a[7]*b.col2[1] + a[11]*b.col2[2] + a[15]*b.col2[3];

        // Fourth col
        res.data[12] = a[0]*b.col3[0] + a[4]*b.col3[1] + a[8]*b.col3[2]  + a[12]*b.col3[3];
        res.data[13] = a[1]*b.col3[0] + a[5]*b.col3[1] + a[9]*b.col3[2]  + a[13]*b.col3[3];
        res.data[14] = a[2]*b.col3[0] + a[6]*b.col3[1] + a[10]*b.col3[2] + a[14]*b.col3[3];
        res.data[15] = a[3]*b.col3[0] + a[7]*b.col3[1] + a[11]*b.col3[2] + a[15]*b.col3[3];

        return res;
    }

    vec4 operator*(const vec4& b) const {
        vec4 res;
        const float* a = this->data;

        res.data[0]  = a[0]*b.data[0] + a[4]*b.data[1] + a[8]*b.data[2]  + a[12]*b.data[3];
        res.data[1]  = a[1]*b.data[0] + a[5]*b.data[1] + a[9]*b.data[2]  + a[13]*b.data[3];
        res.data[2]  = a[2]*b.data[0] + a[6]*b.data[1] + a[10]*b.data[2] + a[14]*b.data[3];
        res.data[3]  = a[3]*b.data[0] + a[7]*b.data[1] + a[11]*b.data[2] + a[15]*b.data[3];

        return res;
    }

    union {
    float data[16];
    struct {
        float col0[4];
        float col1[4];
        float col2[4];
        float col3[4];
    };
    };
};

