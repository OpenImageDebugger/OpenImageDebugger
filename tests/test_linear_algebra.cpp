/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
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

#include <gtest/gtest.h>

// Define M_PI for Windows/MSVC compatibility
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "math/linear_algebra.h"

using namespace oid;

namespace {
constexpr float EPSILON = 1e-5f;
constexpr float PI_OVER_2 = 1.57079632679489661923f; // π/2
constexpr int MATRIX_SIZE = 4;
constexpr float IDENTITY_VALUE = 1.0f;
constexpr float ZERO_VALUE = 0.0f;

// Test data constants
constexpr float TEST_X1 = 1.0f;
constexpr float TEST_Y1 = 2.0f;
constexpr float TEST_Z1 = 3.0f;
constexpr float TEST_W1 = 4.0f;
constexpr float TEST_X2 = 5.0f;
constexpr float TEST_Y2 = 6.0f;
constexpr float TEST_Z2 = 7.0f;
constexpr float TEST_W2 = 8.0f;
constexpr float TEST_SCALE_X = 2.0f;
constexpr float TEST_SCALE_Y = 3.0f;
constexpr float TEST_SCALE_Z = 4.0f;
constexpr float TEST_TRANSLATE_X = 10.0f;
constexpr float TEST_TRANSLATE_Y = 20.0f;
constexpr float TEST_TRANSLATE_Z = 30.0f;
constexpr float TEST_MATRIX_ELEMENT = 5.0f;

// Helper function to verify identity matrix
// Takes a non-const reference because mat4::operator() is not const
void VerifyIdentityMatrix(mat4 &m) {
  for (int i = 0; i < MATRIX_SIZE; ++i) {
    for (int j = 0; j < MATRIX_SIZE; ++j) {
      if (i == j) {
        EXPECT_NEAR(m(i, j), IDENTITY_VALUE, EPSILON);
      } else {
        EXPECT_NEAR(m(i, j), ZERO_VALUE, EPSILON);
      }
    }
  }
}
} // namespace

// vec4 Tests
TEST(Vec4Test, DefaultConstructor) {
  vec4 v;
  EXPECT_FLOAT_EQ(v.x(), 0.0f);
  EXPECT_FLOAT_EQ(v.y(), 0.0f);
  EXPECT_FLOAT_EQ(v.z(), 0.0f);
  EXPECT_FLOAT_EQ(v.w(), 0.0f);
}

TEST(Vec4Test, ParameterizedConstructor) {
  const vec4 v(TEST_X1, TEST_Y1, TEST_Z1, TEST_W1);
  EXPECT_FLOAT_EQ(v.x(), TEST_X1);
  EXPECT_FLOAT_EQ(v.y(), TEST_Y1);
  EXPECT_FLOAT_EQ(v.z(), TEST_Z1);
  EXPECT_FLOAT_EQ(v.w(), TEST_W1);
}

TEST(Vec4Test, Zero) {
  const vec4 v = vec4::zero();
  EXPECT_FLOAT_EQ(v.x(), ZERO_VALUE);
  EXPECT_FLOAT_EQ(v.y(), ZERO_VALUE);
  EXPECT_FLOAT_EQ(v.z(), ZERO_VALUE);
  EXPECT_FLOAT_EQ(v.w(), ZERO_VALUE);
}

TEST(Vec4Test, Addition) {
  const vec4 a(TEST_X1, TEST_Y1, TEST_Z1, TEST_W1);
  const vec4 b(TEST_X2, TEST_Y2, TEST_Z2, TEST_W2);
  const auto c = a + b;

  EXPECT_FLOAT_EQ(c.x(), TEST_X1 + TEST_X2);
  EXPECT_FLOAT_EQ(c.y(), TEST_Y1 + TEST_Y2);
  EXPECT_FLOAT_EQ(c.z(), TEST_Z1 + TEST_Z2);
  EXPECT_FLOAT_EQ(c.w(), TEST_W1 + TEST_W2);
}

TEST(Vec4Test, Subtraction) {
  const vec4 a(TEST_X2, TEST_Y2, TEST_Z2, TEST_W2);
  const vec4 b(TEST_X1, TEST_Y1, TEST_Z1, TEST_W1);
  const auto c = a - b;
  constexpr float expected = TEST_X2 - TEST_X1;
  EXPECT_FLOAT_EQ(c.x(), expected);
  EXPECT_FLOAT_EQ(c.y(), expected);
  EXPECT_FLOAT_EQ(c.z(), expected);
  EXPECT_FLOAT_EQ(c.w(), expected);
}

TEST(Vec4Test, Negation) {
  constexpr float neg_x = 1.0f;
  constexpr float neg_y = -2.0f;
  constexpr float neg_z = 3.0f;
  constexpr float neg_w = -4.0f;
  const vec4 a(neg_x, neg_y, neg_z, neg_w);
  const auto b = -a;

  EXPECT_FLOAT_EQ(b.x(), -neg_x);
  EXPECT_FLOAT_EQ(b.y(), -neg_y);
  EXPECT_FLOAT_EQ(b.z(), -neg_z);
  EXPECT_FLOAT_EQ(b.w(), -neg_w);
}

TEST(Vec4Test, ScalarMultiplication) {
  const vec4 a(TEST_X1, TEST_Y1, TEST_Z1, TEST_W1);
  const auto b = a * TEST_SCALE_X;

  EXPECT_FLOAT_EQ(b.x(), TEST_X1 * TEST_SCALE_X);
  EXPECT_FLOAT_EQ(b.y(), TEST_Y1 * TEST_SCALE_X);
  EXPECT_FLOAT_EQ(b.z(), TEST_Z1 * TEST_SCALE_X);
  EXPECT_FLOAT_EQ(b.w(), TEST_W1 * TEST_SCALE_X);
}

TEST(Vec4Test, PlusEquals) {
  vec4 a(TEST_X1, TEST_Y1, TEST_Z1, TEST_W1);
  const vec4 b(TEST_X2, TEST_Y2, TEST_Z2, TEST_W2);
  a += b;

  EXPECT_FLOAT_EQ(a.x(), TEST_X1 + TEST_X2);
  EXPECT_FLOAT_EQ(a.y(), TEST_Y1 + TEST_Y2);
  EXPECT_FLOAT_EQ(a.z(), TEST_Z1 + TEST_Z2);
  EXPECT_FLOAT_EQ(a.w(), TEST_W1 + TEST_W2);
}

TEST(Vec4Test, DataAccess) {
  vec4 v(TEST_X1, TEST_Y1, TEST_Z1, TEST_W1);
  const float *data = v.data();

  EXPECT_FLOAT_EQ(data[0], TEST_X1);
  EXPECT_FLOAT_EQ(data[1], TEST_Y1);
  EXPECT_FLOAT_EQ(data[2], TEST_Z1);
  EXPECT_FLOAT_EQ(data[3], TEST_W1);
}

TEST(Vec4Test, MutableAccess) {
  vec4 v(TEST_X1, TEST_Y1, TEST_Z1, TEST_W1);
  v.x() = 10.0f;
  v.y() = 20.0f;
  v.z() = 30.0f;
  v.w() = 40.0f;

  EXPECT_FLOAT_EQ(v.x(), 10.0f);
  EXPECT_FLOAT_EQ(v.y(), 20.0f);
  EXPECT_FLOAT_EQ(v.z(), 30.0f);
  EXPECT_FLOAT_EQ(v.w(), 40.0f);
}

// mat4 Tests
TEST(Mat4Test, SetIdentity) {
  mat4 m;
  m.set_identity();
  VerifyIdentityMatrix(m);
}

TEST(Mat4Test, MatrixMultiplication) {
  mat4 a;
  a.set_identity();
  mat4 b;
  b.set_identity();
  auto result = a * b;
  VerifyIdentityMatrix(result);
}

TEST(Mat4Test, MatrixVectorMultiplication) {
  mat4 m;
  m.set_identity();

  const vec4 v(TEST_X1, TEST_Y1, TEST_Z1, TEST_W1);
  const auto result = m * v;

  EXPECT_NEAR(result.x(), TEST_X1, EPSILON);
  EXPECT_NEAR(result.y(), TEST_Y1, EPSILON);
  EXPECT_NEAR(result.z(), TEST_Z1, EPSILON);
  EXPECT_NEAR(result.w(), TEST_W1, EPSILON);
}

TEST(Mat4Test, Translation) {
  const vec4 translation(TEST_TRANSLATE_X, TEST_TRANSLATE_Y, TEST_TRANSLATE_Z,
                         ZERO_VALUE);
  const auto t = mat4::translation(translation);

  const vec4 point(TEST_X1, TEST_Y1, TEST_Z1, IDENTITY_VALUE);
  const auto result = t * point;

  EXPECT_NEAR(result.x(), TEST_X1 + TEST_TRANSLATE_X, EPSILON);
  EXPECT_NEAR(result.y(), TEST_Y1 + TEST_TRANSLATE_Y, EPSILON);
  EXPECT_NEAR(result.z(), TEST_Z1 + TEST_TRANSLATE_Z, EPSILON);
  EXPECT_NEAR(result.w(), IDENTITY_VALUE, EPSILON);
}

TEST(Mat4Test, Scale) {
  const vec4 scale_factor(TEST_SCALE_X, TEST_SCALE_Y, TEST_SCALE_Z,
                          IDENTITY_VALUE);
  const auto s = mat4::scale(scale_factor);

  const vec4 point(TEST_X1, TEST_Y1, TEST_Z1, IDENTITY_VALUE);
  const auto result = s * point;

  EXPECT_NEAR(result.x(), TEST_X1 * TEST_SCALE_X, EPSILON);
  EXPECT_NEAR(result.y(), TEST_Y1 * TEST_SCALE_Y, EPSILON);
  EXPECT_NEAR(result.z(), TEST_Z1 * TEST_SCALE_Z, EPSILON);
  EXPECT_NEAR(result.w(), IDENTITY_VALUE, EPSILON);
}

TEST(Mat4Test, Rotation) {
  constexpr float angle = PI_OVER_2; // 90 degrees
  const auto r = mat4::rotation(angle);

  // Rotate a point on the x-axis by 90 degrees around z-axis
  const vec4 point(IDENTITY_VALUE, ZERO_VALUE, ZERO_VALUE, IDENTITY_VALUE);
  const auto result = r * point;

  // Should end up on the y-axis
  EXPECT_NEAR(result.x(), ZERO_VALUE, EPSILON);
  EXPECT_NEAR(result.y(), IDENTITY_VALUE, EPSILON);
  EXPECT_NEAR(result.z(), ZERO_VALUE, EPSILON);
  EXPECT_NEAR(result.w(), IDENTITY_VALUE, EPSILON);
}

TEST(Mat4Test, SetFromST) {
  mat4 m;
  m.set_from_st(TEST_SCALE_X, TEST_SCALE_Y, TEST_SCALE_Z, TEST_TRANSLATE_X,
                TEST_TRANSLATE_Y, TEST_TRANSLATE_Z);

  const auto *data = m.data();
  EXPECT_NEAR(data[0], TEST_SCALE_X, EPSILON);
  EXPECT_NEAR(data[5], TEST_SCALE_Y, EPSILON);
  EXPECT_NEAR(data[10], TEST_SCALE_Z, EPSILON);
  EXPECT_NEAR(data[12], TEST_TRANSLATE_X, EPSILON);
  EXPECT_NEAR(data[13], TEST_TRANSLATE_Y, EPSILON);
  EXPECT_NEAR(data[14], TEST_TRANSLATE_Z, EPSILON);
  EXPECT_NEAR(data[15], IDENTITY_VALUE, EPSILON);
}

TEST(Mat4Test, SetOrthoProjection) {
  mat4 m;
  constexpr float right = 100.0f;
  constexpr float top = 200.0f;
  constexpr float near_plane = 0.1f;
  constexpr float far_plane = 1000.0f;
  m.set_ortho_projection(right, top, near_plane, far_plane);

  const auto *data = m.data();
  constexpr float expected_scale_x = IDENTITY_VALUE / right;
  constexpr float expected_scale_y = -IDENTITY_VALUE / top;
  constexpr float expected_scale_z = -2.0f / (far_plane - near_plane);
  constexpr float expected_translation_z =
      -(far_plane + near_plane) / (far_plane - near_plane);

  EXPECT_NEAR(data[0], expected_scale_x, EPSILON);
  EXPECT_NEAR(data[5], expected_scale_y, EPSILON);
  EXPECT_NEAR(data[10], expected_scale_z, EPSILON);
  EXPECT_NEAR(data[14], expected_translation_z, EPSILON);
  EXPECT_NEAR(data[15], IDENTITY_VALUE, EPSILON);
}

TEST(Mat4Test, Inverse) {
  mat4 m;
  m.set_identity();
  m(0, 0) = TEST_SCALE_X;
  auto result = m * m.inv();
  VerifyIdentityMatrix(result);
}

TEST(Mat4Test, DataAccess) {
  mat4 m;
  m.set_identity();

  const auto *data = m.data();
  EXPECT_NEAR(data[0], IDENTITY_VALUE, EPSILON);
  EXPECT_NEAR(data[5], IDENTITY_VALUE, EPSILON);
  EXPECT_NEAR(data[10], IDENTITY_VALUE, EPSILON);
  EXPECT_NEAR(data[15], IDENTITY_VALUE, EPSILON);
}

TEST(Mat4Test, OperatorParentheses) {
  mat4 m;
  m.set_identity();

  m(0, 0) = TEST_MATRIX_ELEMENT;
  EXPECT_NEAR(m(0, 0), TEST_MATRIX_ELEMENT, EPSILON);
}
