/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Note: These fixtures test default micro-architecture optimization defined in the
 * util/optimization.h. */

#include "util/math_float3x3.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "util/log.h"
#include "util/transform.h"

#include <iostream>

CCL_NAMESPACE_BEGIN

using testing::Not;

/* Use as:
 * float3 vector1;
 * float3 vector2;
 * EXPECT_THAT(vector1, IsNearFloat3(vector2)); */
MATCHER_P2(IsNearFloat3, expected, eps, "")
{
  for (int i = 0; i < 3; ++i) {
    if (fabsf(arg[i] - expected[i]) > eps) {
      *result_listener << "component " << i << " should be " << expected[i];
      return false;
    }
  }
  return true;
}

/* Use as:
 * float3x3 matrix1;
 * float3x3 matrix2;
 * EXPECT_THAT(matrix1, IsNearFloat3x3(matrix2)); */
MATCHER_P2(IsNearFloat3x3, expected, eps, "")
{
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      if (fabsf(arg[row][col] - expected[row][col]) > eps) {
        *result_listener << "component (" << row << ", " << col << ") should be "
                         << expected[row][col];
        return false;
      }
    }
  }
  return true;
}

/* TODO(sergey): Move to log.h? */
std::ostream &operator<<(std::ostream &os, const float3x3 &m)
{
  os << "(" << m.x << ", " << m.y << ", " << m.z << ")";
  return os;
}

class Float3x3Test : public ::testing::Test {
  void SetUp() override
  {
    /* The micro-architecture check is not needed here, but use it here as a demonstration of how
     * it can be implemented in a clear way. */
    // GTEST_SKIP() << "Test skipped due to uarch capability";
  }
};

/* Basic test for user-defined matcher. */
TEST_F(Float3x3Test, IsNearFloat3x3)
{
  const float3x3 identity = identity_float3x3();
  const float3x3 zero = zero_float3x3();

  EXPECT_THAT(identity, IsNearFloat3x3(identity, 1e-6f));
  EXPECT_THAT(zero, Not(IsNearFloat3x3(identity, 1e-6f)));
}

TEST_F(Float3x3Test, multiply_scalar_by_matrix)
{
  /* >>> import numpy as np
   * >>> a = np.array([[0.1, 0.2, 0.3], [1.1, 1.2, 1.3], [2.1, 2.2, 2.3]])
   * >>> 3.2 * a
   * array([[0.32, 0.64, 0.96],
   *        [3.52, 3.84, 4.16],
   *        [6.72, 7.04, 7.36]]) */
  EXPECT_THAT(3.2f * make_float3x3(make_float3(0.1f, 0.2f, 0.3f),
                                   make_float3(1.1f, 1.2f, 1.3f),
                                   make_float3(2.1f, 2.2f, 2.3f)),
              IsNearFloat3x3(make_float3x3(make_float3(0.32f, 0.64f, 0.96f),
                                           make_float3(3.52f, 3.84f, 4.16f),
                                           make_float3(6.72f, 7.04f, 7.36f)),
                             1e-6f));
}

TEST_F(Float3x3Test, multiply_matrix_by_scalar)
{
  /* >>> import numpy as np
   * >>> a = np.array([[0.1, 0.2, 0.3], [1.1, 1.2, 1.3], [2.1, 2.2, 2.3]])
   * >>> a * 3.2
   * array([[0.32, 0.64, 0.96],
   *        [3.52, 3.84, 4.16],
   *        [6.72, 7.04, 7.36]]) */
  EXPECT_THAT(make_float3x3(make_float3(0.1f, 0.2f, 0.3f),
                            make_float3(1.1f, 1.2f, 1.3f),
                            make_float3(2.1f, 2.2f, 2.3f)) *
                  3.2f,
              IsNearFloat3x3(make_float3x3(make_float3(0.32f, 0.64f, 0.96f),
                                           make_float3(3.52f, 3.84f, 4.16f),
                                           make_float3(6.72f, 7.04f, 7.36f)),
                             1e-6f));
}

TEST_F(Float3x3Test, multiply_matrix_by_matrix)
{
  /* >>> import numpy as np
   * >>> a = np.array([[0.1, 0.2, 0.3], [1.1, 1.2, 1.3], [2.1, 2.2, 2.3]])
   * >>> b = np.array([[3.1, 3.2, 3.3], [4.1, 4.2, 4.3], [5.1, 5.2, 5.3]])
   * >>> a @ b
   * array([[ 2.66,  2.72,  2.78],
   *        [14.96, 15.32, 15.68],
   *        [27.26, 27.92, 28.58]]) */
  EXPECT_THAT(make_float3x3(make_float3(0.1f, 0.2f, 0.3f),
                            make_float3(1.1f, 1.2f, 1.3f),
                            make_float3(2.1f, 2.2f, 2.3f)) *
                  make_float3x3(make_float3(3.1f, 3.2f, 3.3f),
                                make_float3(4.1f, 4.2f, 4.3f),
                                make_float3(5.1f, 5.2f, 5.3f)),
              IsNearFloat3x3(make_float3x3(make_float3(2.66f, 2.72f, 2.78f),
                                           make_float3(14.96f, 15.32f, 15.68f),
                                           make_float3(27.26f, 27.92f, 28.58f)),
                             1e-5f));
}

TEST_F(Float3x3Test, multiply_matrix_by_vector)
{
  /* >>> import numpy as np
   * >>> a = np.array([[0.1, 0.2, 0.3], [1.1, 1.2, 1.3], [2.1, 2.2, 2.3]])
   * >>> b = np.array([3.1, 3.2, 3.3])
   * >>> a @ b
   * array([ 1.94, 11.54, 21.14]) */
  EXPECT_THAT(make_float3x3(make_float3(0.1f, 0.2f, 0.3f),
                            make_float3(1.1f, 1.2f, 1.3f),
                            make_float3(2.1f, 2.2f, 2.3f)) *
                  make_float3(3.1f, 3.2f, 3.3f),
              IsNearFloat3(make_float3(1.94f, 11.54f, 21.14f), 1e-6f));
}

TEST_F(Float3x3Test, divide_matrix_by_scalar)
{
  /* >>> import numpy as np
   * >>> a = np.array([[0.1, 0.2, 0.3], [1.1, 1.2, 1.3], [2.1, 2.2, 2.3]])
   * >>> a / 3.2
   * array([[0.03125, 0.0625 , 0.09375],
   *        [0.34375, 0.375  , 0.40625],
   *        [0.65625, 0.6875 , 0.71875]]) */
  EXPECT_THAT(make_float3x3(make_float3(0.1f, 0.2f, 0.3f),
                            make_float3(1.1f, 1.2f, 1.3f),
                            make_float3(2.1f, 2.2f, 2.3f)) /
                  3.2f,
              IsNearFloat3x3(make_float3x3(make_float3(0.03125f, 0.0625f, 0.09375f),
                                           make_float3(0.34375f, 0.375f, 0.40625f),
                                           make_float3(0.65625f, 0.6875f, 0.71875f)),
                             1e-6f));
}

TEST_F(Float3x3Test, float3x3_scale)
{
  EXPECT_THAT(float3x3_scale(make_float3(0.1f, 1.2f, 2.3f)),
              IsNearFloat3x3(make_float3x3(make_float3(0.1f, 0.0f, 0.0f),
                                           make_float3(0.0f, 1.2f, 0.0f),
                                           make_float3(0.0f, 0.0f, 2.3f)),
                             1e-6f));
}

TEST_F(Float3x3Test, transposed)
{
  EXPECT_THAT(transposed(make_float3x3(make_float3(0.0f, 0.1f, 0.2f),
                                       make_float3(1.0f, 1.1f, 1.2f),
                                       make_float3(2.0f, 2.1f, 2.2f))),
              IsNearFloat3x3(make_float3x3(make_float3(0.0f, 1.0f, 2.0f),
                                           make_float3(0.1f, 1.1f, 2.1f),
                                           make_float3(0.2f, 1.2f, 2.2f)),
                             1e-6f));
}

TEST_F(Float3x3Test, determinant)
{
  /* >>> import numpy as np
   * >>> a = np.array([[0.1, 0.2, 0.3], [1.3, 1.2, 1.1], [2.2, 2.1, 2.3]])
   * >>> np.linalg.det(a)
   * np.float64(-0.04199999999999992) */
  EXPECT_NEAR(determinant(make_float3x3(make_float3(0.1f, 0.2f, 0.3f),
                                        make_float3(1.3f, 1.2f, 1.1f),
                                        make_float3(2.2f, 2.1f, 2.3f))),
              -0.042f,
              1e-6f);
}

TEST_F(Float3x3Test, adjoint)
{
  EXPECT_THAT(adjoint(make_float3x3(make_float3(-3.0f, 2.0f, -5.0f),
                                    make_float3(-1.0f, 0.0f, -2.0f),
                                    make_float3(3.0f, -4.0f, 1.0f))),
              IsNearFloat3x3(make_float3x3(make_float3(-8.0f, 18.0f, -4.0f),
                                           make_float3(-5.0f, 12.0f, -1.0f),
                                           make_float3(4.0f, -6.0f, 2.0f)),
                             1e-6f));
}

TEST_F(Float3x3Test, inverted)
{
  /* >>> import numpy as np
   * >>> a = np.array([[0.1, 0.2, 0.3], [1.3, 1.2, 1.1], [2.2, 2.1, 2.3]])
   * >>> np.linalg.inv(a)
   * array([[-10.71428571,  -4.04761905,   3.33333333],
            [ 13.57142857,  10.23809524,  -6.66666667],
            [ -2.14285714,  -5.47619048,   3.33333333]]) */
  const float3x3 matrix = make_float3x3(
      make_float3(0.1f, 0.2f, 0.3f), make_float3(1.3f, 1.2f, 1.1f), make_float3(2.2f, 2.1f, 2.3f));
  EXPECT_THAT(inverted(matrix),
              IsNearFloat3x3(make_float3x3(make_float3(-10.71428571f, -4.04761905f, 3.33333333f),
                                           make_float3(13.57142857f, 10.23809524f, -6.66666667f),
                                           make_float3(-2.14285714f, -5.47619048f, 3.33333333f)),
                             1e-4f));
  EXPECT_THAT(matrix * inverted(matrix), IsNearFloat3x3(identity_float3x3(), 1e-5f));
  EXPECT_THAT(inverted(matrix) * matrix, IsNearFloat3x3(identity_float3x3(), 1e-5f));
}

TEST_F(Float3x3Test, quaternion_to_scaled_rotation)
{
  EXPECT_THAT(quaternion_to_scaled_rotation(
                  make_quaternion(2.0f * cosf(M_PI_F / 4.0f), 0, 0, 2.0f * sinf(M_PI_F / 4.0f))),
              IsNearFloat3x3(make_float3x3(make_float3(0.0f, -4.0f, 0.0f),
                                           make_float3(4.0f, 0.0f, 0.0f),
                                           make_float3(0.0f, 0.0f, 4.0f)),
                             1e-6f));
}

TEST_F(Float3x3Test, quaternion_to_rotation)
{
  /* >>> from scipy.spatial.transform import Rotation as R
   * >>> import numpy as np
   * >>> r = R.from_quat([np.cos(np.pi/4), 0, 0, np.sin(np.pi/4)], scalar_first=True)
   * >>> r.as_matrix()
   * array([[ 2.22044605e-16, -1.00000000e+00,  0.00000000e+00],
   *        [ 1.00000000e+00,  2.22044605e-16,  0.00000000e+00],
   *        [ 0.00000000e+00,  0.00000000e+00,  1.00000000e+00]]) */
  EXPECT_THAT(
      quaternion_to_rotation(make_quaternion(cosf(M_PI_F / 4.0f), 0, 0, sinf(M_PI_F / 4.0f))),
      IsNearFloat3x3(make_float3x3(make_float3(0.0f, -1.0f, 0.0f),
                                   make_float3(1.0f, 0.0f, 0.0f),
                                   make_float3(0.0f, 0.0f, 1.0f)),
                     1e-6f));
  EXPECT_THAT(quaternion_to_rotation(
                  make_quaternion(2.0f * cosf(M_PI_F / 4.0f), 0, 0, 2.0f * sinf(M_PI_F / 4.0f))),
              IsNearFloat3x3(make_float3x3(make_float3(0.0f, -1.0f, 0.0f),
                                           make_float3(1.0f, 0.0f, 0.0f),
                                           make_float3(0.0f, 0.0f, 1.0f)),
                             1e-6f));

  {
    Transform tfm = transform_euler(make_float3(0.1f, -0.2f, 0.3f));
    const float4 q = transform_to_quat(tfm);
    const float3x3 R = quaternion_to_rotation(make_quaternion(q.w, q.x, q.y, q.z));
    EXPECT_THAT(R.x, IsNearFloat3(tfm.x, 1e-6f));
    EXPECT_THAT(R.y, IsNearFloat3(tfm.y, 1e-6f));
    EXPECT_THAT(R.z, IsNearFloat3(tfm.z, 1e-6f));
  }
}

class PackedFloat3x3Test : public Float3x3Test {};

TEST_F(PackedFloat3x3Test, init_from_float3x3)
{
  const packed_float3x3 packed(make_float3x3(make_float3(0.0f, 0.1f, 0.2f),
                                             make_float3(1.0f, 1.1f, 1.2f),
                                             make_float3(2.0f, 2.1f, 2.2f)));
  EXPECT_NEAR(packed.x.x, 0.0f, 1e-6f);
  EXPECT_NEAR(packed.x.y, 0.1f, 1e-6f);
  EXPECT_NEAR(packed.x.z, 0.2f, 1e-6f);

  EXPECT_NEAR(packed.y.x, 1.0f, 1e-6f);
  EXPECT_NEAR(packed.y.y, 1.1f, 1e-6f);
  EXPECT_NEAR(packed.y.z, 1.2f, 1e-6f);

  EXPECT_NEAR(packed.z.x, 2.0f, 1e-6f);
  EXPECT_NEAR(packed.z.y, 2.1f, 1e-6f);
  EXPECT_NEAR(packed.z.z, 2.2f, 1e-6f);
}

TEST_F(PackedFloat3x3Test, cast_to_float3x3)
{
  const packed_float3x3 packed(make_float3x3(make_float3(0.0f, 0.1f, 0.2f),
                                             make_float3(1.0f, 1.1f, 1.2f),
                                             make_float3(2.0f, 2.1f, 2.2f)));
  const float3x3 unpacked = packed;
  EXPECT_THAT(unpacked,
              IsNearFloat3x3(make_float3x3(make_float3(0.0f, 0.1f, 0.2f),
                                           make_float3(1.0f, 1.1f, 1.2f),
                                           make_float3(2.0f, 2.1f, 2.2f)),
                             1e-6f));
}

TEST_F(PackedFloat3x3Test, assign_from_float3x3)
{
  packed_float3x3 packed = identity_float3x3();
  packed = make_float3x3(
      make_float3(0.0f, 0.1f, 0.2f), make_float3(1.0f, 1.1f, 1.2f), make_float3(2.0f, 2.1f, 2.2f));
  EXPECT_THAT(float3x3(packed),
              IsNearFloat3x3(make_float3x3(make_float3(0.0f, 0.1f, 0.2f),
                                           make_float3(1.0f, 1.1f, 1.2f),
                                           make_float3(2.0f, 2.1f, 2.2f)),
                             1e-6f));
}

CCL_NAMESPACE_END
