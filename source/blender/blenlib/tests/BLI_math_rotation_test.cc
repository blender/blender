/* SPDX-License-Identifier: Apache-2.0 */

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
  EXPECT_EQ(eul.x, 0.0f);
  EXPECT_EQ(eul.y, 0.0f);
  EXPECT_EQ(eul.z, 0.0f);
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
  AxisAngle a({0.0f, 0.0f, 2.0f}, M_PI_2);
  EXPECT_V3_NEAR(a.axis(), float3(0, 0, 1), 1e-4);
  EXPECT_NEAR(a.angle(), M_PI_2, 1e-4);

  AxisAngleNormalized b({0.0f, 0.0f, 1.0f}, M_PI_2);
  EXPECT_V3_NEAR(b.axis(), float3(0, 0, 1), 1e-4);
  EXPECT_NEAR(b.angle(), M_PI_2, 1e-4);

  AxisAngle c({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
  EXPECT_V3_NEAR(c.axis(), float3(0, 0, 1), 1e-4);
  EXPECT_NEAR(c.angle(), M_PI_2, 1e-4);

  AxisAngle d({1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f});
  EXPECT_V3_NEAR(d.axis(), float3(0, 0, -1), 1e-4);
  EXPECT_NEAR(d.angle(), M_PI_2, 1e-4);
}

TEST(math_rotation, TypeConversion)
{
  EulerXYZ euler(0, 0, M_PI_2);
  Quaternion quat(M_SQRT1_2, 0.0f, 0.0f, M_SQRT1_2);
  AxisAngle axis_angle({0.0f, 0.0f, 2.0f}, M_PI_2);

  EXPECT_V4_NEAR(float4(Quaternion(euler)), float4(quat), 1e-4);
  EXPECT_V3_NEAR(AxisAngle(euler).axis(), axis_angle.axis(), 1e-4);
  EXPECT_NEAR(AxisAngle(euler).angle(), axis_angle.angle(), 1e-4);

  EXPECT_V3_NEAR(float3(EulerXYZ(quat)), float3(euler), 1e-4);
  EXPECT_V3_NEAR(AxisAngle(quat).axis(), axis_angle.axis(), 1e-4);
  EXPECT_NEAR(AxisAngle(quat).angle(), axis_angle.angle(), 1e-4);

  EXPECT_V3_NEAR(float3(EulerXYZ(axis_angle)), float3(euler), 1e-4);
  EXPECT_V4_NEAR(float4(Quaternion(axis_angle)), float4(quat), 1e-4);
}

}  // namespace blender::math::tests
