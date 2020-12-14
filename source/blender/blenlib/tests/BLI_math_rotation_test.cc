/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_math_base.h"
#include "BLI_math_rotation.h"

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
