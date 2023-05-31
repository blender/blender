/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_base.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_rotation.hh"
#include "BLI_math_rotation_legacy.hh"
#include "BLI_math_vector.hh"

#include "BLI_vector.hh"

#include <cmath>

/* Test that quaternion converts to itself via matrix. */
static void test_quat_to_mat_to_quat(float w, float x, float y, float z)
{
  float in_quat[4] = {w, x, y, z};
  float norm_quat[4], matrix[3][3], out_quat[4];

  normalize_qt_qt(norm_quat, in_quat);
  quat_to_mat3(matrix, norm_quat);
  mat3_normalized_to_quat(out_quat, matrix);

  /* The expected result is flipped (each orientation corresponds to 2 quats) */
  if (w < 0) {
    mul_qt_fl(norm_quat, -1);
  }

  EXPECT_V4_NEAR(norm_quat, out_quat, FLT_EPSILON);
}

TEST(math_rotation, quat_to_mat_to_quat_rot180)
{
  test_quat_to_mat_to_quat(1, 0, 0, 0);
  test_quat_to_mat_to_quat(0, 1, 0, 0);
  test_quat_to_mat_to_quat(0, 0, 1, 0);
  test_quat_to_mat_to_quat(0, 0, 0, 1);
}

TEST(math_rotation, quat_to_mat_to_quat_rot180n)
{
  test_quat_to_mat_to_quat(-1.000f, 0, 0, 0);
  test_quat_to_mat_to_quat(-1e-20f, -1, 0, 0);
  test_quat_to_mat_to_quat(-1e-20f, 0, -1, 0);
  test_quat_to_mat_to_quat(-1e-20f, 0, 0, -1);
}

TEST(math_rotation, quat_to_mat_to_quat_rot90)
{
  const float s2 = 1 / sqrtf(2);
  test_quat_to_mat_to_quat(s2, s2, 0, 0);
  test_quat_to_mat_to_quat(s2, -s2, 0, 0);
  test_quat_to_mat_to_quat(s2, 0, s2, 0);
  test_quat_to_mat_to_quat(s2, 0, -s2, 0);
  test_quat_to_mat_to_quat(s2, 0, 0, s2);
  test_quat_to_mat_to_quat(s2, 0, 0, -s2);
}

TEST(math_rotation, quat_to_mat_to_quat_rot90n)
{
  const float s2 = 1 / sqrtf(2);
  test_quat_to_mat_to_quat(-s2, s2, 0, 0);
  test_quat_to_mat_to_quat(-s2, -s2, 0, 0);
  test_quat_to_mat_to_quat(-s2, 0, s2, 0);
  test_quat_to_mat_to_quat(-s2, 0, -s2, 0);
  test_quat_to_mat_to_quat(-s2, 0, 0, s2);
  test_quat_to_mat_to_quat(-s2, 0, 0, -s2);
}

TEST(math_rotation, quat_to_mat_to_quat_bad_T83196)
{
  test_quat_to_mat_to_quat(0.0032f, 0.9999f, -0.0072f, -0.0100f);
  test_quat_to_mat_to_quat(0.0058f, 0.9999f, -0.0090f, -0.0101f);
  test_quat_to_mat_to_quat(0.0110f, 0.9998f, -0.0140f, -0.0104f);
  test_quat_to_mat_to_quat(0.0142f, 0.9997f, -0.0192f, -0.0107f);
  test_quat_to_mat_to_quat(0.0149f, 0.9996f, -0.0212f, -0.0107f);
}

TEST(math_rotation, quat_to_mat_to_quat_bad_negative)
{
  /* This shouldn't produce a negative q[0]. */
  test_quat_to_mat_to_quat(0.5f - 1e-6f, 0, -sqrtf(3) / 2 - 1e-6f, 0);
}

TEST(math_rotation, quat_to_mat_to_quat_near_1000)
{
  test_quat_to_mat_to_quat(0.9999f, 0.01f, -0.001f, -0.01f);
  test_quat_to_mat_to_quat(0.9999f, 0.02f, -0.002f, -0.02f);
  test_quat_to_mat_to_quat(0.9999f, 0.03f, -0.003f, -0.03f);
  test_quat_to_mat_to_quat(0.9999f, 0.04f, -0.004f, -0.04f);
  test_quat_to_mat_to_quat(0.9999f, 0.05f, -0.005f, -0.05f);
  test_quat_to_mat_to_quat(0.999f, 0.10f, -0.010f, -0.10f);
  test_quat_to_mat_to_quat(0.99f, 0.15f, -0.015f, -0.15f);
  test_quat_to_mat_to_quat(0.98f, 0.20f, -0.020f, -0.20f);
  test_quat_to_mat_to_quat(0.97f, 0.25f, -0.025f, -0.25f);
  test_quat_to_mat_to_quat(0.95f, 0.30f, -0.030f, -0.30f);
}

TEST(math_rotation, quat_to_mat_to_quat_near_0100)
{
  test_quat_to_mat_to_quat(0.01f, 0.9999f, -0.001f, -0.01f);
  test_quat_to_mat_to_quat(0.02f, 0.9999f, -0.002f, -0.02f);
  test_quat_to_mat_to_quat(0.03f, 0.9999f, -0.003f, -0.03f);
  test_quat_to_mat_to_quat(0.04f, 0.9999f, -0.004f, -0.04f);
  test_quat_to_mat_to_quat(0.05f, 0.9999f, -0.005f, -0.05f);
  test_quat_to_mat_to_quat(0.10f, 0.999f, -0.010f, -0.10f);
  test_quat_to_mat_to_quat(0.15f, 0.99f, -0.015f, -0.15f);
  test_quat_to_mat_to_quat(0.20f, 0.98f, -0.020f, -0.20f);
  test_quat_to_mat_to_quat(0.25f, 0.97f, -0.025f, -0.25f);
  test_quat_to_mat_to_quat(0.30f, 0.95f, -0.030f, -0.30f);
}

TEST(math_rotation, quat_to_mat_to_quat_near_0010)
{
  test_quat_to_mat_to_quat(0.01f, -0.001f, 0.9999f, -0.01f);
  test_quat_to_mat_to_quat(0.02f, -0.002f, 0.9999f, -0.02f);
  test_quat_to_mat_to_quat(0.03f, -0.003f, 0.9999f, -0.03f);
  test_quat_to_mat_to_quat(0.04f, -0.004f, 0.9999f, -0.04f);
  test_quat_to_mat_to_quat(0.05f, -0.005f, 0.9999f, -0.05f);
  test_quat_to_mat_to_quat(0.10f, -0.010f, 0.999f, -0.10f);
  test_quat_to_mat_to_quat(0.15f, -0.015f, 0.99f, -0.15f);
  test_quat_to_mat_to_quat(0.20f, -0.020f, 0.98f, -0.20f);
  test_quat_to_mat_to_quat(0.25f, -0.025f, 0.97f, -0.25f);
  test_quat_to_mat_to_quat(0.30f, -0.030f, 0.95f, -0.30f);
}

TEST(math_rotation, quat_to_mat_to_quat_near_0001)
{
  test_quat_to_mat_to_quat(0.01f, -0.001f, -0.01f, 0.9999f);
  test_quat_to_mat_to_quat(0.02f, -0.002f, -0.02f, 0.9999f);
  test_quat_to_mat_to_quat(0.03f, -0.003f, -0.03f, 0.9999f);
  test_quat_to_mat_to_quat(0.04f, -0.004f, -0.04f, 0.9999f);
  test_quat_to_mat_to_quat(0.05f, -0.005f, -0.05f, 0.9999f);
  test_quat_to_mat_to_quat(0.10f, -0.010f, -0.10f, 0.999f);
  test_quat_to_mat_to_quat(0.15f, -0.015f, -0.15f, 0.99f);
  test_quat_to_mat_to_quat(0.20f, -0.020f, -0.20f, 0.98f);
  test_quat_to_mat_to_quat(0.25f, -0.025f, -0.25f, 0.97f);
  test_quat_to_mat_to_quat(0.30f, -0.030f, -0.30f, 0.95f);
}

/* A zeroed matrix converted to a quaternion and back should not add rotation, see: #101848 */
TEST(math_rotation, quat_to_mat_to_quat_zeroed_matrix)
{
  float matrix_zeroed[3][3] = {{0.0f}};
  float matrix_result[3][3];
  float matrix_unit[3][3];
  float out_quat[4];

  unit_m3(matrix_unit);
  mat3_normalized_to_quat(out_quat, matrix_zeroed);
  quat_to_mat3(matrix_result, out_quat);

  EXPECT_M3_NEAR(matrix_unit, matrix_result, FLT_EPSILON);
}

TEST(math_rotation, quat_split_swing_and_twist_negative)
{
  const float input[4] = {-0.5f, 0, sqrtf(3) / 2, 0};
  const float expected_swing[4] = {1.0f, 0, 0, 0};
  const float expected_twist[4] = {0.5f, 0, -sqrtf(3) / 2, 0};
  float swing[4], twist[4];

  float twist_angle = quat_split_swing_and_twist(input, 1, swing, twist);

  EXPECT_NEAR(twist_angle, -M_PI * 2 / 3, FLT_EPSILON);
  EXPECT_V4_NEAR(swing, expected_swing, FLT_EPSILON);
  EXPECT_V4_NEAR(twist, expected_twist, FLT_EPSILON);
}

TEST(math_rotation, mat3_normalized_to_quat_fast_degenerate)
{
  /* This input will cause floating point issues, which would produce a non-unit
   * quaternion if the call to `normalize_qt` were to be removed. This
   * particular matrix was taken from a production file of Pet Projects that
   * caused problems. */
  const float input[3][3] = {
      {1.0000000000, -0.0000006315, -0.0000000027},
      {0.0000009365, 1.0000000000, -0.0000000307},
      {0.0000001964, 0.2103530765, 0.9776254892},
  };
  const float expect_quat[4] = {
      0.99860459566116333,
      -0.052810292690992355,
      4.9985139582986449e-08,
      -3.93654971730939e-07,
  };
  ASSERT_FLOAT_EQ(1.0f, dot_qtqt(expect_quat, expect_quat))
      << "expected quaternion should be normal";

  float actual_quat[4];
  mat3_normalized_to_quat_fast(actual_quat, input);
  EXPECT_FLOAT_EQ(1.0f, dot_qtqt(actual_quat, actual_quat));
  EXPECT_V4_NEAR(expect_quat, actual_quat, FLT_EPSILON);
}

/* -------------------------------------------------------------------- */
/** \name Test `sin_cos_from_fraction` Accuracy & Exact Symmetry
 * \{ */

static void test_sin_cos_from_fraction_accuracy(const int range, const float expected_eps)
{
  for (int i = 0; i < range; i++) {
    float sin_cos_fl[2];
    sin_cos_from_fraction(i, range, &sin_cos_fl[0], &sin_cos_fl[1]);
    const float phi = float(2.0 * M_PI) * (float(i) / float(range));
    const float sin_cos_test_fl[2] = {sinf(phi), cosf(phi)};
    EXPECT_V2_NEAR(sin_cos_fl, sin_cos_test_fl, expected_eps);
  }
}

/** Ensure the result of #sin_cos_from_fraction match #sinf & #cosf. */
TEST(math_rotation, sin_cos_from_fraction_accuracy)
{
  for (int range = 1; range <= 64; range++) {
    test_sin_cos_from_fraction_accuracy(range, 1e-6f);
  }
}

/** Ensure values are exactly symmetrical where possible. */
static void test_sin_cos_from_fraction_symmetry(const int range)
{
  /* The expected number of unique numbers depends on the range being a multiple of 4/2/1. */
  const enum {
    MULTIPLE_OF_1 = 1,
    MULTIPLE_OF_2 = 2,
    MULTIPLE_OF_4 = 3,
  } multiple_of = (range & 1) ? MULTIPLE_OF_1 : ((range & 3) ? MULTIPLE_OF_2 : MULTIPLE_OF_4);

  blender::Vector<blender::float2> coords;
  coords.reserve(range);
  for (int i = 0; i < range; i++) {
    float sin_cos_fl[2];
    sin_cos_from_fraction(i, range, &sin_cos_fl[0], &sin_cos_fl[1]);
    switch (multiple_of) {
      case MULTIPLE_OF_1: {
        sin_cos_fl[0] = fabsf(sin_cos_fl[0]);
        break;
      }
      case MULTIPLE_OF_2: {
        sin_cos_fl[0] = fabsf(sin_cos_fl[0]);
        sin_cos_fl[1] = fabsf(sin_cos_fl[1]);
        break;
      }
      case MULTIPLE_OF_4: {
        sin_cos_fl[0] = fabsf(sin_cos_fl[0]);
        sin_cos_fl[1] = fabsf(sin_cos_fl[1]);
        if (sin_cos_fl[0] > sin_cos_fl[1]) {
          std::swap(sin_cos_fl[0], sin_cos_fl[1]);
        }
        break;
      }
    }
    coords.append_unchecked(sin_cos_fl);
  }
  /* Sort, then count unique items. */
  std::sort(coords.begin(), coords.end(), [](const blender::float2 &a, const blender::float2 &b) {
    float delta = b[0] - a[0];
    if (delta == 0.0f) {
      delta = b[1] - a[1];
    }
    return delta > 0.0f;
  });
  int unique_coords_count = 1;
  if (range > 1) {
    int i_prev = 0;
    for (int i = 1; i < range; i_prev = i++) {
      if (coords[i_prev] != coords[i]) {
        unique_coords_count += 1;
      }
    }
  }
  switch (multiple_of) {
    case MULTIPLE_OF_1: {
      EXPECT_EQ(unique_coords_count, (range / 2) + 1);
      break;
    }
    case MULTIPLE_OF_2: {
      EXPECT_EQ(unique_coords_count, (range / 4) + 1);
      break;
    }
    case MULTIPLE_OF_4: {
      EXPECT_EQ(unique_coords_count, (range / 8) + 1);
      break;
    }
  }
}

TEST(math_rotation, sin_cos_from_fraction_symmetry)
{
  for (int range = 1; range <= 64; range++) {
    test_sin_cos_from_fraction_symmetry(range);
  }
}

/** \} */

namespace blender::math::tests {

TEST(math_rotation, DefaultConstructor)
{
  Quaternion quat{};
  EXPECT_EQ(quat.x, 0.0f);
  EXPECT_EQ(quat.y, 0.0f);
  EXPECT_EQ(quat.z, 0.0f);
  EXPECT_EQ(quat.w, 0.0f);

  EulerXYZ eul{};
  EXPECT_EQ(eul.x(), 0.0f);
  EXPECT_EQ(eul.y(), 0.0f);
  EXPECT_EQ(eul.z(), 0.0f);
}

TEST(math_rotation, RotateDirectionAroundAxis)
{
  const float3 a = rotate_direction_around_axis({1, 0, 0}, {0, 0, 1}, M_PI_2);
  EXPECT_NEAR(a.x, 0.0f, FLT_EPSILON);
  EXPECT_NEAR(a.y, 1.0f, FLT_EPSILON);
  EXPECT_NEAR(a.z, 0.0f, FLT_EPSILON);
  const float3 b = rotate_direction_around_axis({1, 0, 0}, {0, 0, 1}, M_PI);
  EXPECT_NEAR(b.x, -1.0f, FLT_EPSILON);
  EXPECT_NEAR(b.y, 0.0f, FLT_EPSILON);
  EXPECT_NEAR(b.z, 0.0f, FLT_EPSILON);
  const float3 c = rotate_direction_around_axis({0, 0, 1}, {0, 0, 1}, 0.0f);
  EXPECT_NEAR(c.x, 0.0f, FLT_EPSILON);
  EXPECT_NEAR(c.y, 0.0f, FLT_EPSILON);
  EXPECT_NEAR(c.z, 1.0f, FLT_EPSILON);
}

TEST(math_rotation, AxisAngleConstructors)
{
  AxisAngle a({0.0f, 0.0f, 1.0f}, M_PI_2);
  EXPECT_V3_NEAR(a.axis(), float3(0, 0, 1), 1e-4);
  EXPECT_NEAR(float(a.angle()), M_PI_2, 1e-4);
  EXPECT_NEAR(sin(a.angle()), 1.0f, 1e-4);
  EXPECT_NEAR(cos(a.angle()), 0.0f, 1e-4);

  AxisAngleCartesian b({0.0f, 0.0f, 1.0f}, AngleCartesian(AngleRadian(M_PI_2)));
  EXPECT_V3_NEAR(b.axis(), float3(0, 0, 1), 1e-4);
  EXPECT_NEAR(float(b.angle()), M_PI_2, 1e-4);
  EXPECT_NEAR(b.angle().sin(), 1.0f, 1e-4);
  EXPECT_NEAR(b.angle().cos(), 0.0f, 1e-4);

  AxisAngle axis_angle_basis = AxisAngle(AxisSigned::Y_NEG, M_PI);
  EXPECT_EQ(axis_angle_basis.axis(), float3(0.0f, -1.0f, 0.0f));
  EXPECT_EQ(axis_angle_basis.angle(), M_PI);

  AxisAngle c({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
  EXPECT_V3_NEAR(c.axis(), float3(0, 0, 1), 1e-4);
  EXPECT_NEAR(float(c.angle()), M_PI_2, 1e-4);

  AxisAngle d({1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f});
  EXPECT_V3_NEAR(d.axis(), float3(0, 0, -1), 1e-4);
  EXPECT_NEAR(float(d.angle()), M_PI_2, 1e-4);
}

TEST(math_rotation, QuaternionDot)
{
  Quaternion q1(1.0f, 2.0f, 3.0f, 4.0f);
  Quaternion q2(2.0f, -3.0f, 5.0f, 100.0f);
  EXPECT_EQ(math::dot(q1, q2), 411.0f);
}

TEST(math_rotation, QuaternionConjugate)
{
  Quaternion q1(1.0f, 2.0f, 3.0f, 4.0f);
  EXPECT_EQ(float4(conjugate(q1)), float4(1.0f, -2.0f, -3.0f, -4.0f));
}

TEST(math_rotation, QuaternionNormalize)
{
  Quaternion q1(1.0f, 2.0f, 3.0f, 4.0f);
  EXPECT_V4_NEAR(float4(normalize(q1)),
                 float4(0.1825741827, 0.3651483654, 0.5477225780, 0.7302967309),
                 1e-6f);
}

TEST(math_rotation, QuaternionInvert)
{
  Quaternion q1(1.0f, 2.0f, 3.0f, 4.0f);
  EXPECT_V4_NEAR(float4(invert(q1)), float4(0.0333333f, -0.0666667f, -0.1f, -0.133333f), 1e-4f);

  Quaternion q2(0.927091f, 0.211322f, -0.124857f, 0.283295f);
  Quaternion result = invert_normalized(normalize(q2));
  EXPECT_V4_NEAR(float4(result), float4(0.927091f, -0.211322f, 0.124857f, -0.283295f), 1e-4f);
}

TEST(math_rotation, QuaternionCanonicalize)
{
  EXPECT_V4_NEAR(float4(canonicalize(Quaternion(0.5f, 2.0f, 3.0f, 4.0f))),
                 float4(0.5f, 2.0f, 3.0f, 4.0f),
                 1e-4f);
  EXPECT_V4_NEAR(float4(canonicalize(Quaternion(-0.5f, 2.0f, 3.0f, 4.0f))),
                 float4(0.5f, -2.0f, -3.0f, -4.0f),
                 1e-4f);
}

TEST(math_rotation, QuaternionAngleBetween)
{
  Quaternion q1 = normalize(Quaternion(0.927091f, 0.211322f, -0.124857f, 0.283295f));
  Quaternion q2 = normalize(Quaternion(-0.083377f, -0.051681f, 0.498261f, -0.86146f));
  Quaternion q3 = rotation_between(q1, q2);
  EXPECT_V4_NEAR(float4(q3), float4(-0.394478f, 0.00330195f, 0.284119f, -0.873872f), 1e-4f);
  EXPECT_NEAR(float(angle_of(q1)), 0.76844f, 1e-4f);
  EXPECT_NEAR(float(angle_of(q2)), 3.30854f, 1e-4f);
  EXPECT_NEAR(float(angle_of(q3)), 3.95259f, 1e-4f);
  EXPECT_NEAR(float(angle_of_signed(q1)), 0.76844f, 1e-4f);
  EXPECT_NEAR(float(angle_of_signed(q2)), 3.30854f - 2 * M_PI, 1e-4f);
  EXPECT_NEAR(float(angle_of_signed(q3)), 3.95259f - 2 * M_PI, 1e-4f);
  EXPECT_NEAR(float(angle_between(q1, q2)), 3.95259f, 1e-4f);
  EXPECT_NEAR(float(angle_between_signed(q1, q2)), 3.95259f - 2 * M_PI, 1e-4f);
}

TEST(math_rotation, QuaternionPower)
{
  Quaternion q1 = normalize(Quaternion(0.927091f, 0.211322f, -0.124857f, 0.283295f));
  Quaternion q2 = normalize(Quaternion(-0.083377f, -0.051681f, 0.498261f, -0.86146f));

  EXPECT_V4_NEAR(
      float4(math::pow(q1, -2.5f)), float4(0.573069, -0.462015, 0.272976, -0.61937), 1e-4);
  EXPECT_V4_NEAR(
      float4(math::pow(q1, -0.5f)), float4(0.981604, -0.107641, 0.0635985, -0.144302), 1e-4);
  EXPECT_V4_NEAR(
      float4(math::pow(q1, 0.5f)), float4(0.981604, 0.107641, -0.0635985, 0.144302), 1e-4);
  EXPECT_V4_NEAR(
      float4(math::pow(q1, 2.5f)), float4(0.573069, 0.462015, -0.272976, 0.61937), 1e-4);
  EXPECT_V4_NEAR(
      float4(math::pow(q2, -2.5f)), float4(-0.545272, -0.0434735, 0.419131, -0.72465), 1e-4);
  EXPECT_V4_NEAR(
      float4(math::pow(q2, -0.5f)), float4(0.676987, 0.0381699, -0.367999, 0.636246), 1e-4);
  EXPECT_V4_NEAR(
      float4(math::pow(q2, 0.5f)), float4(0.676987, -0.0381699, 0.367999, -0.636246), 1e-4);
  EXPECT_V4_NEAR(
      float4(math::pow(q2, 2.5f)), float4(-0.545272, 0.0434735, -0.419131, 0.72465), 1e-4);
}

TEST(math_rotation, QuaternionFromTriangle)
{
  float3 v1(0.927091f, 0.211322f, -0.124857f);
  float3 v2(-0.051681f, 0.498261f, -0.86146f);
  float3 v3(0.211322f, -0.124857f, 0.283295f);
  EXPECT_V4_NEAR(float4(from_triangle(v1, v2, v3)),
                 float4(0.255566f, -0.213799f, 0.454253f, 0.826214f),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_triangle(v1, v3, v2)),
                 float4(0.103802f, 0.295067f, -0.812945f, 0.491204f),
                 1e-5f);
}

TEST(math_rotation, QuaternionFromVector)
{
  float3 v1(0.927091f, 0.211322f, -0.124857f);
  float3 v2(-0.051681f, 0.498261f, -0.86146f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::X_POS, Axis::X)),
                 float4(0.129047, 0, -0.50443, -0.853755),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::X_POS, Axis::Y)),
                 float4(0.12474, 0.0330631, -0.706333, -0.696017),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::X_POS, Axis::Z)),
                 float4(0.111583, -0.0648251, -0.00729451, -0.991612),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::Y_POS, Axis::X)),
                 float4(0.476074, 0.580363, -0.403954, 0.522832),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::Y_POS, Axis::Y)),
                 float4(0.62436, 0.104259, 0, 0.774148),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::Y_POS, Axis::Z)),
                 float4(0.622274, 0.0406802, 0.0509963, 0.780077),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::Z_POS, Axis::X)),
                 float4(0.747014, 0.0737433, -0.655337, 0.0840594),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::Z_POS, Axis::Z)),
                 float4(0.751728, 0.146562, -0.642981, 0),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::Z_POS, Axis::Z)),
                 float4(0.751728, 0.146562, -0.642981, 0),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::X_NEG, Axis::X)),
                 float4(0.991638, 0, 0.0656442, 0.111104),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::X_NEG, Axis::Y)),
                 float4(0.706333, 0.696017, 0.12474, 0.0330631),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::X_NEG, Axis::Z)),
                 float4(0.991612, -0.0072946, 0.0648251, 0.111583),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::Y_NEG, Axis::X)),
                 float4(0.580363, -0.476074, -0.522832, -0.403954),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::Y_NEG, Axis::Y)),
                 float4(0.781137, -0.083334, 0, -0.618774),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::Y_NEG, Axis::Z)),
                 float4(0.780077, -0.0509963, 0.0406802, -0.622274),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::Z_NEG, Axis::X)),
                 float4(0.0737433, -0.747014, -0.0840594, -0.655337),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::Z_NEG, Axis::Z)),
                 float4(0.659473, -0.167065, 0.732929, 0),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v1, AxisSigned::Z_NEG, Axis::Z)),
                 float4(0.659473, -0.167065, 0.732929, 0),
                 1e-5f);

  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::X_POS, Axis::X)),
                 float4(0.725211, 0, -0.596013, -0.344729),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::X_POS, Axis::Y)),
                 float4(0.691325, 0.219092, -0.672309, -0.148561),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::X_POS, Axis::Z)),
                 float4(0.643761, -0.333919, -0.370346, -0.580442),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::Y_POS, Axis::X)),
                 float4(0.320473, 0.593889, 0.383792, 0.630315),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::Y_POS, Axis::Y)),
                 float4(0.499999, 0.864472, 0, -0.0518617),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::Y_POS, Axis::Z)),
                 float4(0.0447733, 0.0257574, -0.49799, -0.865643),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::Z_POS, Axis::X)),
                 float4(0.646551, 0.193334, -0.174318, 0.717082),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::Z_POS, Axis::Z)),
                 float4(0.965523, 0.258928, 0.0268567, 0),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::Z_POS, Axis::Z)),
                 float4(0.965523, 0.258928, 0.0268567, 0),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::X_NEG, Axis::X)),
                 float4(0.688527, 0, 0.627768, 0.363095),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::X_NEG, Axis::Y)),
                 float4(0.672309, 0.148561, 0.691325, 0.219092),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::X_NEG, Axis::Z)),
                 float4(0.580442, -0.370345, 0.333919, 0.643761),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::Y_NEG, Axis::X)),
                 float4(0.593889, -0.320473, -0.630315, 0.383792),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::Y_NEG, Axis::Y)),
                 float4(0.866026, -0.499102, 0, 0.0299423),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::Y_NEG, Axis::Z)),
                 float4(0.865643, -0.49799, -0.0257574, 0.0447733),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::Z_NEG, Axis::X)),
                 float4(0.193334, -0.646551, -0.717082, -0.174318),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::Z_NEG, Axis::Z)),
                 float4(0.260317, -0.960371, -0.0996123, 0),
                 1e-5f);
  EXPECT_V4_NEAR(float4(from_vector(v2, AxisSigned::Z_NEG, Axis::Z)),
                 float4(0.260317, -0.960371, -0.0996123, 0),
                 1e-5f);
}

TEST(math_rotation, QuaternionWrappedAround)
{
  Quaternion q1 = normalize(Quaternion(0.927091f, 0.211322f, -0.124857f, 0.283295f));
  Quaternion q2 = normalize(Quaternion(-0.083377f, -0.051681f, 0.498261f, -0.86146f));
  Quaternion q_malformed = Quaternion(0.0f, 0.0f, 0.0f, 0.0f);
  EXPECT_V4_NEAR(float4(q1.wrapped_around(q2)), float4(-q1), 1e-4f);
  EXPECT_V4_NEAR(float4(q1.wrapped_around(-q2)), float4(q1), 1e-4f);
  EXPECT_V4_NEAR(float4(q1.wrapped_around(q_malformed)), float4(q1), 1e-4f);
}

TEST(math_rotation, QuaternionFromTracking)
{
  for (int i : IndexRange(6)) {
    for (int j : IndexRange(3)) {
      AxisSigned forward_axis = AxisSigned::from_int(i);
      Axis up_axis = Axis::from_int(j);

      if (forward_axis.axis() == up_axis) {
        continue;
      }

      Quaternion expect = Quaternion::identity();
      quat_apply_track(&expect.w, forward_axis.as_int(), up_axis.as_int());

      /* This is the expected axis conversion for curve tangent space to tracked object space. */
      CartesianBasis axes = rotation_between(
          from_orthonormal_axes(AxisSigned::Z_POS, AxisSigned::Y_POS),
          from_orthonormal_axes(forward_axis, AxisSigned(up_axis)));
      Quaternion result = to_quaternion<float>(axes);

      EXPECT_V4_NEAR(float4(result), float4(expect), 1e-5f);
    }
  }
}

TEST(math_rotation, EulerWrappedAround)
{
  EulerXYZ eul1 = EulerXYZ(2.08542, -1.12485, -1.23738);
  EulerXYZ eul2 = EulerXYZ(4.06112, 0.561928, -18.9063);
  EXPECT_V3_NEAR(float3(eul1.wrapped_around(eul2)), float3(2.08542, -1.12485, -20.0869), 1e-4f);
  EXPECT_V3_NEAR(float3(eul2.wrapped_around(eul1)), float3(4.06112, 0.561928, -0.0567436), 1e-4f);
}

TEST(math_rotation, Euler3ToGimbal)
{
  /* All the same rotation. */
  float3 ijk{0.350041, -0.358896, 0.528994};
  Euler3 euler3_xyz(ijk, EulerOrder::XYZ);
  Euler3 euler3_xzy(ijk, EulerOrder::XZY);
  Euler3 euler3_yxz(ijk, EulerOrder::YXZ);
  Euler3 euler3_yzx(ijk, EulerOrder::YZX);
  Euler3 euler3_zxy(ijk, EulerOrder::ZXY);
  Euler3 euler3_zyx(ijk, EulerOrder::ZYX);

  float3x3 mat_xyz = transpose(
      float3x3({0.808309, -0.504665, 0}, {0.47251, 0.863315, 0}, {0.351241, 0, 1}));
  float3x3 mat_xzy = transpose(
      float3x3({0.808309, 0, -0.351241}, {0.504665, 1, -0}, {0.303232, 0, 0.936285}));
  float3x3 mat_yxz = transpose(
      float3x3({0.863315, -0.474062, 0}, {0.504665, 0.810963, 0}, {-0, 0.342936, 1}));
  float3x3 mat_yzx = transpose(
      float3x3({1, -0.504665, 0}, {0, 0.810963, -0.342936}, {0, 0.296062, 0.939359}));
  float3x3 mat_zxy = transpose(
      float3x3({0.936285, 0, -0.329941}, {0, 1, -0.342936}, {0.351241, 0, 0.879508}));
  float3x3 mat_zyx = transpose(
      float3x3({1, -0, -0.351241}, {0, 0.939359, -0.321086}, {0, 0.342936, 0.879508}));

  EXPECT_M3_NEAR(to_gimbal_axis(euler3_xyz), mat_xyz, 1e-4);
  EXPECT_M3_NEAR(to_gimbal_axis(euler3_xzy), mat_xzy, 1e-4);
  EXPECT_M3_NEAR(to_gimbal_axis(euler3_yxz), mat_yxz, 1e-4);
  EXPECT_M3_NEAR(to_gimbal_axis(euler3_yzx), mat_yzx, 1e-4);
  EXPECT_M3_NEAR(to_gimbal_axis(euler3_zxy), mat_zxy, 1e-4);
  EXPECT_M3_NEAR(to_gimbal_axis(euler3_zyx), mat_zyx, 1e-4);
}

TEST(math_rotation, CartesianBasis)
{
  for (int i : IndexRange(6)) {
    for (int j : IndexRange(6)) {
      for (int k : IndexRange(6)) {
        for (int l : IndexRange(6)) {
          AxisSigned src_forward = AxisSigned::from_int(i);
          AxisSigned src_up = AxisSigned::from_int(j);
          AxisSigned dst_forward = AxisSigned::from_int(k);
          AxisSigned dst_up = AxisSigned::from_int(l);

          if ((abs(src_forward) == abs(src_up)) || (abs(dst_forward) == abs(dst_up))) {
            /* Assertion expected. */
            continue;
          }

          float3x3 expect;
          if (src_forward == dst_forward && src_up == dst_up) {
            expect = float3x3::identity();
          }
          else {
            /* TODO: Find a way to test without resorting to old C API. */
            mat3_from_axis_conversion(src_forward.as_int(),
                                      src_up.as_int(),
                                      dst_forward.as_int(),
                                      dst_up.as_int(),
                                      expect.ptr());
          }

          CartesianBasis rotation = rotation_between(from_orthonormal_axes(src_forward, src_up),
                                                     from_orthonormal_axes(dst_forward, dst_up));
          EXPECT_EQ(from_rotation<float3x3>(rotation), expect);

          if (src_forward == dst_forward) {
            expect = float3x3::identity();
          }
          else {
            /* TODO: Find a way to test without resorting to old C API. */
            mat3_from_axis_conversion_single(
                src_forward.as_int(), dst_forward.as_int(), expect.ptr());
          }

          EXPECT_EQ(from_rotation<float3x3>(rotation_between(src_forward, dst_forward)), expect);

          float3 point(1.0f, 2.0f, 3.0f);
          CartesianBasis rotation_inv = invert(rotation);
          /* Test inversion identity. */
          EXPECT_EQ(transform_point(rotation_inv, transform_point(rotation, point)), point);
        }
      }
    }
  }
}

TEST(math_rotation, Transform)
{
  Quaternion q(0.927091f, 0.211322f, -0.124857f, 0.283295f);

  float3 p(0.576f, -0.6546f, 46.354f);
  float3 result = transform_point(q, p);
  EXPECT_V3_NEAR(result, float3(-4.33722f, -21.661f, 40.7608f), 1e-4f);

  /* Validated using `to_quaternion` before doing the transform. */
  float3 p2(1.0f, 2.0f, 3.0f);
  result = transform_point(CartesianBasis(AxisSigned::X_POS, AxisSigned::Y_POS, AxisSigned::Z_POS),
                           p2);
  EXPECT_EQ(result, float3(1.0f, 2.0f, 3.0f));
  result = transform_point(
      rotation_between(from_orthonormal_axes(AxisSigned::Y_POS, AxisSigned::Z_POS),
                       from_orthonormal_axes(AxisSigned::X_POS, AxisSigned::Z_POS)),
      p2);
  EXPECT_EQ(result, float3(-2.0f, 1.0f, 3.0f));
  result = transform_point(from_orthonormal_axes(AxisSigned::Z_POS, AxisSigned::X_POS), p2);
  EXPECT_EQ(result, float3(3.0f, 1.0f, 2.0f));
  result = transform_point(from_orthonormal_axes(AxisSigned::X_NEG, AxisSigned::Y_POS), p2);
  EXPECT_EQ(result, float3(-2.0f, 3.0f, -1.0f));
}

TEST(math_rotation, DualQuaternionNormalize)
{
  DualQuaternion sum = DualQuaternion(Quaternion(0, 0, 1, 0), Quaternion(0, 1, 0, 1)) * 2.0f;
  sum += DualQuaternion(Quaternion(1, 0, 0, 0), Quaternion(1, 1, 1, 1), float4x4::identity()) *
         4.0f;
  sum += DualQuaternion(Quaternion(1, 0, 0, 0), Quaternion(1, 0, 0, 0), float4x4::identity()) *
         3.0f;

  sum = normalize(sum);

  /* The difference with the C API. */
  float len = length(float4(0.777778, 0, 0.222222, 0));

  EXPECT_V4_NEAR(float4(sum.quat), (float4(0.777778, 0, 0.222222, 0) / len), 1e-4f);
  EXPECT_V4_NEAR(float4(sum.trans), (float4(0.777778, 0.666667, 0.444444, 0.666667) / len), 1e-4f);
  EXPECT_EQ(sum.scale, float4x4::identity());
  EXPECT_EQ(sum.scale_weight, 1.0f);
  EXPECT_EQ(sum.quat_weight, 1.0f);
}

TEST(math_rotation, DualQuaternionFromMatrix)
{
  {
    float4x4 mat{transpose(float4x4({-2.14123, -0.478481, -1.38296, -2.26029},
                                    {-1.28264, 2.87361, 0.0230992, 12.8871},
                                    {3.27343, 0.812993, -0.895575, -13.5216},
                                    {0, 0, 0, 1}))};
    float4x4 basemat{transpose(float4x4({0.0988318, 0.91328, 0.39516, 7.73971},
                                        {0.16104, -0.406549, 0.899324, 22.8871},
                                        {0.981987, -0.0252451, -0.187255, -3.52155},
                                        {0, 0, 0, 1}))};
    float4x4 expected_scale_mat{transpose(float4x4({4.08974, 0.306437, -0.0853435, -31.2277},
                                                   {-0.445021, 2.97151, -0.250095, -42.5586},
                                                   {0.146173, 0.473002, 1.62645, -9.75092},
                                                   {0, 0, 0, 1}))};

    DualQuaternion dq = to_dual_quaternion(mat, basemat);
    EXPECT_V4_NEAR(float4(dq.quat), float4(0.502368, 0.0543716, -0.854483, -0.120535), 1e-4f);
    EXPECT_V4_NEAR(float4(dq.trans), float4(22.674, -0.878616, 11.2762, 14.167), 1e-4f);
    EXPECT_M4_NEAR(dq.scale, expected_scale_mat, 1e-4f);
    EXPECT_EQ(dq.scale_weight, 1.0f);
    EXPECT_EQ(dq.quat_weight, 1.0f);
  }
  {
    float4x4 mat{transpose(float4x4({-0.0806635, -1.60529, 2.44763, 26.823},
                                    {-1.04583, -0.150756, -0.385074, -22.2225},
                                    {-0.123402, 2.32698, 1.66357, 5.397},
                                    {0, 0, 0, 1}))};
    float4x4 basemat{transpose(float4x4({0.0603774, 0.904674, 0.421806, 36.823},
                                        {-0.271734, 0.421514, -0.865151, -12.2225},
                                        {-0.960477, -0.0623834, 0.27128, 15.397},
                                        {0, 0, 0, 1}))};
    float4x4 expected_scale_mat{transpose(float4x4({0.248852, 2.66363, -0.726295, 71.3985},
                                                   {0.971507, -0.382422, 1.09917, -69.5943},
                                                   {-0.331274, 0.8794, 2.67787, -2.88715},
                                                   {0, 0, 0, 1}))};

    DualQuaternion dq = to_dual_quaternion(mat, basemat);
    EXPECT_V4_NEAR(float4(dq.quat), float4(0.149898, -0.319339, -0.0441496, -0.934668), 1e-4f);
    EXPECT_V4_NEAR(float4(dq.trans), float4(-2.20019, 39.6236, 49.052, -16.2077), 1e-4f);
    EXPECT_M4_NEAR(dq.scale, expected_scale_mat, 1e-4f);
    EXPECT_EQ(dq.scale_weight, 1.0f);
    EXPECT_EQ(dq.quat_weight, 1.0f);
  }

#if 0 /* Generate random matrices. */
  for (int i = 0; i < 1000; i++) {
    auto frand = []() { return (std::rand() - RAND_MAX / 2) / float(RAND_MAX); };
    float4x4 mat = from_loc_rot_scale<float4x4>(
        float3{frand(), frand(), frand()} * 100.0f,
        EulerXYZ{frand() * 10.0f, frand() * 10.0f, frand() * 10.0f},
        float3{frand(), frand(), frand()} * 10.0f);
    float4x4 basemat = from_loc_rot<float4x4>(
        mat.location() + 10, EulerXYZ{frand() * 10.0f, frand() * 10.0f, frand() * 10.0f});

    DualQuaternion expect;
    mat4_to_dquat((DualQuat *)&expect.quat.w, basemat.ptr(), mat.ptr());

    DualQuaternion dq = to_dual_quaternion(mat, basemat);
    EXPECT_V4_NEAR(float4(dq.quat), float4(expect.quat), 1e-4f);
    EXPECT_V4_NEAR(float4(dq.trans), float4(expect.trans), 1e-4f);
    EXPECT_M4_NEAR(dq.scale, expect.scale, 2e-4f);
    EXPECT_EQ(dq.scale_weight, expect.scale_weight);
  }
#endif
}

TEST(math_rotation, DualQuaternionTransform)
{
  {
    float4x4 scale_mat{transpose(float4x4({4.08974, 0.306437, -0.0853435, -31.2277},
                                          {-0.445021, 2.97151, -0.250095, -42.5586},
                                          {0.146173, 0.473002, 1.62645, -9.75092},
                                          {0, 0, 0, 1}))};

    DualQuaternion dq({0.502368, 0.0543716, -0.854483, -0.120535},
                      {22.674, -0.878616, 11.2762, 14.167},
                      scale_mat);

    float3 p0{51.0f, 1647.0f, 12.0f};
    float3 p1{58.0f, 0.0054f, 10.0f};
    float3 p2{0.0f, 7854.0f, 111.0f};

    float3x3 crazy_space_mat;
    float3 p0_expect = p0;
    float3 p1_expect = p1;
    float3 p2_expect = p2;
    mul_v3m3_dq(p0_expect, crazy_space_mat.ptr(), (DualQuat *)&dq);
    mul_v3m3_dq(p1_expect, crazy_space_mat.ptr(), (DualQuat *)&dq);
    mul_v3m3_dq(p2_expect, crazy_space_mat.ptr(), (DualQuat *)&dq);

    float3 p0_result = transform_point(dq, p0);
    float3 p1_result = transform_point(dq, p1);
    float3 p2_result = transform_point(dq, p2, &crazy_space_mat);

    float4x4 expected_crazy_space_mat{transpose(float3x3({-2.14123, -0.478481, -1.38296},
                                                         {-1.28264, 2.87361, 0.0230978},
                                                         {3.27343, 0.812991, -0.895574}))};

    EXPECT_V3_NEAR(p0_result, p0_expect, 1e-2f);
    EXPECT_V3_NEAR(p1_result, p1_expect, 1e-2f);
    EXPECT_V3_NEAR(p2_result, p2_expect, 1e-2f);
    EXPECT_M3_NEAR(crazy_space_mat, expected_crazy_space_mat, 1e-4f);
  }
  {
    float4x4 scale_mat{transpose(float4x4({0.248852, 2.66363, -0.726295, 71.3985},
                                          {0.971507, -0.382422, 1.09917, -69.5943},
                                          {-0.331274, 0.8794, 2.67787, -2.88715},
                                          {0, 0, 0, 1}))};

    DualQuaternion dq({0.149898, -0.319339, -0.0441496, -0.934668},
                      {-2.20019, 39.6236, 49.052, -16.207},
                      scale_mat);

    float3 p0{51.0f, 1647.0f, 12.0f};
    float3 p1{58.0f, 0.0054f, 10.0f};
    float3 p2{0.0f, 7854.0f, 111.0f};

    float3x3 crazy_space_mat;
    float3 p0_expect = p0;
    float3 p1_expect = p1;
    float3 p2_expect = p2;
    mul_v3m3_dq(p0_expect, crazy_space_mat.ptr(), (DualQuat *)&dq);
    mul_v3m3_dq(p1_expect, crazy_space_mat.ptr(), (DualQuat *)&dq);
    mul_v3m3_dq(p2_expect, crazy_space_mat.ptr(), (DualQuat *)&dq);

    float3 p0_result = transform_point(dq, p0);
    float3 p1_result = transform_point(dq, p1);
    float3 p2_result = transform_point(dq, p2, &crazy_space_mat);

    float4x4 expected_crazy_space_mat{transpose(float3x3({-0.0806647, -1.60529, 2.44763},
                                                         {-1.04583, -0.150754, -0.385079},
                                                         {-0.123401, 2.32698, 1.66357}))};

    EXPECT_V3_NEAR(p0_result, float3(-2591.83, -328.472, 3851.6), 1e-2f);
    EXPECT_V3_NEAR(p1_result, float3(46.6121, -86.7318, 14.8882), 1e-2f);
    EXPECT_V3_NEAR(p2_result, float3(-12309.5, -1248.99, 18466.1), 6e-2f);
    EXPECT_M3_NEAR(crazy_space_mat, expected_crazy_space_mat, 1e-4f);
  }
}

}  // namespace blender::math::tests
