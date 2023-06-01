/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_rotation.hh"

TEST(math_matrix, interp_m4_m4m4_regular)
{
  /* Test 4x4 matrix interpolation without singularity, i.e. without axis flip. */

  /* Transposed matrix, so that the code here is written in the same way as print_m4() outputs. */
  /* This matrix represents T=(0.1, 0.2, 0.3), R=(40, 50, 60) degrees, S=(0.7, 0.8, 0.9) */
  float matrix_a[4][4] = {
      {0.224976f, -0.333770f, 0.765074f, 0.100000f},
      {0.389669f, 0.647565f, 0.168130f, 0.200000f},
      {-0.536231f, 0.330541f, 0.443163f, 0.300000f},
      {0.000000f, 0.000000f, 0.000000f, 1.000000f},
  };
  transpose_m4(matrix_a);

  float matrix_i[4][4];
  unit_m4(matrix_i);

  float result[4][4];
  const float epsilon = 1e-6;
  interp_m4_m4m4(result, matrix_i, matrix_a, 0.0f);
  EXPECT_M4_NEAR(result, matrix_i, epsilon);

  interp_m4_m4m4(result, matrix_i, matrix_a, 1.0f);
  EXPECT_M4_NEAR(result, matrix_a, epsilon);

  /* This matrix is based on the current implementation of the code, and isn't guaranteed to be
   * correct. It's just consistent with the current implementation. */
  float matrix_halfway[4][4] = {
      {0.690643f, -0.253244f, 0.484996f, 0.050000f},
      {0.271924f, 0.852623f, 0.012348f, 0.100000f},
      {-0.414209f, 0.137484f, 0.816778f, 0.150000f},
      {0.000000f, 0.000000f, 0.000000f, 1.000000f},
  };

  transpose_m4(matrix_halfway);
  interp_m4_m4m4(result, matrix_i, matrix_a, 0.5f);
  EXPECT_M4_NEAR(result, matrix_halfway, epsilon);
}

TEST(math_matrix, interp_m3_m3m3_singularity)
{
  /* A singularity means that there is an axis mirror in the rotation component of the matrix.
   * This is reflected in its negative determinant.
   *
   * The interpolation of 4x4 matrices performs linear interpolation on the translation component,
   * and then uses the 3x3 interpolation function to handle rotation and scale. As a result, this
   * test for a singularity in the rotation matrix only needs to test the 3x3 case. */

  /* Transposed matrix, so that the code here is written in the same way as print_m4() outputs. */
  /* This matrix represents R=(4, 5, 6) degrees, S=(-1, 1, 1) */
  float matrix_a[3][3] = {
      {-0.990737f, -0.098227f, 0.093759f},
      {-0.104131f, 0.992735f, -0.060286f},
      {0.087156f, 0.069491f, 0.993768f},
  };
  transpose_m3(matrix_a);
  EXPECT_NEAR(-1.0f, determinant_m3_array(matrix_a), 1e-6);

  /* This matrix represents R=(0, 0, 0), S=(-1, 1, 1) */
  float matrix_b[3][3] = {
      {-1.0f, 0.0f, 0.0f},
      {0.0f, 1.0f, 0.0f},
      {0.0f, 0.0f, 1.0f},
  };
  transpose_m3(matrix_b);

  float result[3][3];
  interp_m3_m3m3(result, matrix_a, matrix_b, 0.0f);
  EXPECT_M3_NEAR(result, matrix_a, 1e-5);

  interp_m3_m3m3(result, matrix_a, matrix_b, 1.0f);
  EXPECT_M3_NEAR(result, matrix_b, 1e-5);

  interp_m3_m3m3(result, matrix_a, matrix_b, 0.5f);
  float expect[3][3] = {
      {-0.997681f, -0.049995f, 0.046186f},
      {-0.051473f, 0.998181f, -0.031385f},
      {0.044533f, 0.033689f, 0.998440f},
  };
  transpose_m3(expect);
  EXPECT_M3_NEAR(result, expect, 1e-5);

  /* Interpolating between a matrix with and without axis flip can cause it to go through a zero
   * point. The determinant det(A) of a matrix represents the change in volume; interpolating
   * between matrices with det(A)=-1 and det(B)=1 will have to go through a point where
   * det(result)=0, so where the volume becomes zero. */
  float matrix_i[3][3];
  unit_m3(matrix_i);
  zero_m3(expect);
  interp_m3_m3m3(result, matrix_a, matrix_i, 0.5f);
  EXPECT_NEAR(0.0f, determinant_m3_array(result), 1e-5);
  EXPECT_M3_NEAR(result, expect, 1e-5);
}

TEST(math_matrix, mul_m3_series)
{
  float matrix[3][3] = {
      {2.0f, 0.0f, 0.0f},
      {0.0f, 3.0f, 0.0f},
      {0.0f, 0.0f, 5.0f},
  };
  mul_m3_series(matrix, matrix, matrix, matrix);
  float expect[3][3] = {
      {8.0f, 0.0f, 0.0f},
      {0.0f, 27.0f, 0.0f},
      {0.0f, 0.0f, 125.0f},
  };
  EXPECT_M3_NEAR(matrix, expect, 1e-5);
}

TEST(math_matrix, mul_m4_series)
{
  float matrix[4][4] = {
      {2.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 3.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 5.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 7.0f},
  };
  mul_m4_series(matrix, matrix, matrix, matrix);
  float expect[4][4] = {
      {8.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 27.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 125.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 343.0f},
  };
  EXPECT_M4_NEAR(matrix, expect, 1e-5);
}

namespace blender::tests {

using namespace blender::math;

TEST(math_matrix, MatrixInverse)
{
  float3x3 mat = float3x3::diagonal(2);
  float3x3 inv = invert(mat);
  float3x3 expect = float3x3({0.5f, 0.0f, 0.0f}, {0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.5f});
  EXPECT_M3_NEAR(inv, expect, 1e-5f);

  bool success;
  float3x3 mat2 = float3x3::all(1);
  float3x3 inv2 = invert(mat2, success);
  float3x3 expect2 = float3x3::all(0);
  EXPECT_M3_NEAR(inv2, expect2, 1e-5f);
  EXPECT_FALSE(success);
}

TEST(math_matrix, MatrixPseudoInverse)
{
  float4x4 mat = transpose(float4x4({0.224976f, -0.333770f, 0.765074f, 0.100000f},
                                    {0.389669f, 0.647565f, 0.168130f, 0.200000f},
                                    {-0.536231f, 0.330541f, 0.443163f, 0.300000f},
                                    {0.000000f, 0.000000f, 0.000000f, 1.000000f}));
  float4x4 expect = transpose(float4x4({0.224976f, -0.333770f, 0.765074f, 0.100000f},
                                       {0.389669f, 0.647565f, 0.168130f, 0.200000f},
                                       {-0.536231f, 0.330541f, 0.443163f, 0.300000f},
                                       {0.000000f, 0.000000f, 0.000000f, 1.000000f}));
  float4x4 inv = pseudo_invert(mat);
  pseudoinverse_m4_m4(expect.ptr(), mat.ptr(), 1e-8f);
  EXPECT_M4_NEAR(inv, expect, 1e-5f);

  float4x4 mat2 = transpose(float4x4({0.000000f, -0.333770f, 0.765074f, 0.100000f},
                                     {0.000000f, 0.647565f, 0.168130f, 0.200000f},
                                     {0.000000f, 0.330541f, 0.443163f, 0.300000f},
                                     {0.000000f, 0.000000f, 0.000000f, 1.000000f}));
  float4x4 expect2 = transpose(float4x4({0.000000f, 0.000000f, 0.000000f, 0.000000f},
                                        {-0.51311f, 1.02638f, 0.496437f, -0.302896f},
                                        {0.952803f, 0.221885f, 0.527413f, -0.297881f},
                                        {-0.0275438f, -0.0477073f, 0.0656508f, 0.9926f}));
  float4x4 inv2 = pseudo_invert(mat2);
  EXPECT_M4_NEAR(inv2, expect2, 1e-5f);
}

TEST(math_matrix, MatrixDeterminant)
{
  float2x2 m2({1, 2}, {3, 4});
  float3x3 m3({1, 2, 3}, {-3, 4, -5}, {5, -6, 7});
  float4x4 m4({1, 2, -3, 3}, {3, 4, -5, 3}, {5, 6, 7, -3}, {5, 6, 7, 1});
  EXPECT_NEAR(determinant(m2), -2.0f, 1e-8f);
  EXPECT_NEAR(determinant(m3), -16.0f, 1e-8f);
  EXPECT_NEAR(determinant(m4), -112.0f, 1e-8f);
  EXPECT_NEAR(determinant(double2x2(m2)), -2.0f, 1e-8f);
  EXPECT_NEAR(determinant(double3x3(m3)), -16.0f, 1e-8f);
  EXPECT_NEAR(determinant(double4x4(m4)), -112.0f, 1e-8f);
}

TEST(math_matrix, MatrixAdjoint)
{
  float2x2 m2({1, 2}, {3, 4});
  float3x3 m3({1, 2, 3}, {-3, 4, -5}, {5, -6, 7});
  float4x4 m4({1, 2, -3, 3}, {3, 4, -5, 3}, {5, 6, 7, -3}, {5, 6, 7, 1});
  float2x2 expect2 = transpose(float2x2({4, -3}, {-2, 1}));
  float3x3 expect3 = transpose(float3x3({-2, -4, -2}, {-32, -8, 16}, {-22, -4, 10}));
  float4x4 expect4 = transpose(
      float4x4({232, -184, -8, -0}, {-128, 88, 16, 0}, {80, -76, 4, 28}, {-72, 60, -12, -28}));
  EXPECT_M2_NEAR(adjoint(m2), expect2, 1e-8f);
  EXPECT_M3_NEAR(adjoint(m3), expect3, 1e-8f);
  EXPECT_M4_NEAR(adjoint(m4), expect4, 1e-8f);
}

TEST(math_matrix, MatrixAccess)
{
  float4x4 m({1, 2, 3, 4}, {5, 6, 7, 8}, {9, 1, 2, 3}, {4, 5, 6, 7});
  /** Access helpers. */
  EXPECT_EQ(m.x_axis(), float3(1, 2, 3));
  EXPECT_EQ(m.y_axis(), float3(5, 6, 7));
  EXPECT_EQ(m.z_axis(), float3(9, 1, 2));
  EXPECT_EQ(m.location(), float3(4, 5, 6));
}

TEST(math_matrix, MatrixInit)
{
  float4x4 expect;

  float4x4 m = from_location<float4x4>({1, 2, 3});
  expect = float4x4({1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {1, 2, 3, 1});
  EXPECT_TRUE(is_equal(m, expect, 0.00001f));

  expect = transpose(float4x4({0.411982, -0.833738, -0.36763, 0},
                              {-0.0587266, -0.426918, 0.902382, 0},
                              {-0.909297, -0.350175, -0.224845, 0},
                              {0, 0, 0, 1}));
  EulerXYZ euler(1, 2, 3);
  Quaternion quat = to_quaternion(euler);
  AxisAngle axis_angle = to_axis_angle(euler);
  m = from_rotation<float4x4>(euler);
  EXPECT_M3_NEAR(m, expect, 1e-5);
  m = from_rotation<float4x4>(quat);
  EXPECT_M3_NEAR(m, expect, 1e-5);
  m = from_rotation<float4x4>(axis_angle);
  EXPECT_M3_NEAR(m, expect, 1e-5);

  expect = transpose(float4x4({0.823964, -1.66748, -0.735261, 3.28334},
                              {-0.117453, -0.853835, 1.80476, 5.44925},
                              {-1.81859, -0.700351, -0.44969, -0.330972},
                              {0, 0, 0, 1}));
  DualQuaternion dual_quat(quat, Quaternion(0.5f, 0.5f, 0.5f, 1.5f), float4x4::diagonal(2.0f));
  m = from_rotation<float4x4>(dual_quat);
  EXPECT_M3_NEAR(m, expect, 1e-5);

  m = from_scale<float4x4>(float4(1, 2, 3, 4));
  expect = float4x4({1, 0, 0, 0}, {0, 2, 0, 0}, {0, 0, 3, 0}, {0, 0, 0, 4});
  EXPECT_TRUE(is_equal(m, expect, 0.00001f));

  m = from_scale<float4x4>(float3(1, 2, 3));
  expect = float4x4({1, 0, 0, 0}, {0, 2, 0, 0}, {0, 0, 3, 0}, {0, 0, 0, 1});
  EXPECT_TRUE(is_equal(m, expect, 0.00001f));

  m = from_scale<float4x4>(float2(1, 2));
  expect = float4x4({1, 0, 0, 0}, {0, 2, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1});
  EXPECT_TRUE(is_equal(m, expect, 0.00001f));

  m = from_loc_rot<float4x4>({1, 2, 3}, EulerXYZ{1, 2, 3});
  expect = float4x4({0.411982, -0.0587266, -0.909297, 0},
                    {-0.833738, -0.426918, -0.350175, 0},
                    {-0.36763, 0.902382, -0.224845, 0},
                    {1, 2, 3, 1});
  EXPECT_TRUE(is_equal(m, expect, 0.00001f));

  m = from_loc_rot_scale<float4x4>({1, 2, 3}, EulerXYZ{1, 2, 3}, float3{1, 2, 3});
  expect = float4x4({0.411982, -0.0587266, -0.909297, 0},
                    {-1.66748, -0.853835, -0.700351, 0},
                    {-1.10289, 2.70714, -0.674535, 0},
                    {1, 2, 3, 1});
  EXPECT_TRUE(is_equal(m, expect, 0.00001f));
}

TEST(math_matrix, MatrixModify)
{
  const float epsilon = 1e-6;
  float4x4 result, expect;
  float4x4 m1 = float4x4({0, 3, 0, 0}, {2, 0, 0, 0}, {0, 0, 2, 0}, {0, 0, 0, 1});

  expect = float4x4({0, 3, 0, 0}, {2, 0, 0, 0}, {0, 0, 2, 0}, {4, 9, 2, 1});
  result = translate(m1, float3(3, 2, 1));
  EXPECT_M4_NEAR(result, expect, epsilon);

  expect = float4x4({0, 3, 0, 0}, {2, 0, 0, 0}, {0, 0, 2, 0}, {4, 0, 0, 1});
  result = translate(m1, float2(0, 2));
  EXPECT_M4_NEAR(result, expect, epsilon);

  expect = float4x4({0, 0, -2, 0}, {2, 0, 0, 0}, {0, 3, 0, 0}, {0, 0, 0, 1});
  result = rotate(m1, AxisAngle({0, 1, 0}, M_PI_2));
  EXPECT_M4_NEAR(result, expect, epsilon);

  expect = float4x4({0, 9, 0, 0}, {4, 0, 0, 0}, {0, 0, 8, 0}, {0, 0, 0, 1});
  result = scale(m1, float3(3, 2, 4));
  EXPECT_M4_NEAR(result, expect, epsilon);

  expect = float4x4({0, 9, 0, 0}, {4, 0, 0, 0}, {0, 0, 2, 0}, {0, 0, 0, 1});
  result = scale(m1, float2(3, 2));
  EXPECT_M4_NEAR(result, expect, epsilon);
}

TEST(math_matrix, MatrixCompareTest)
{
  float4x4 m1 = float4x4({0, 3, 0, 0}, {2, 0, 0, 0}, {0, 0, 2, 0}, {0, 0, 0, 1});
  float4x4 m2 = float4x4({0, 3.001, 0, 0}, {1.999, 0, 0, 0}, {0, 0, 2.001, 0}, {0, 0, 0, 1.001});
  float4x4 m3 = float4x4({0, 3.001, 0, 0}, {1, 1, 0, 0}, {0, 0, 2.001, 0}, {0, 0, 0, 1.001});
  float4x4 m4 = float4x4({0, 1, 0, 0}, {1, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1});
  float4x4 m5 = float4x4({0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0});
  float4x4 m6 = float4x4({1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1});
  EXPECT_TRUE(is_equal(m1, m2, 0.01f));
  EXPECT_FALSE(is_equal(m1, m2, 0.0001f));
  EXPECT_FALSE(is_equal(m1, m3, 0.01f));
  EXPECT_TRUE(is_orthogonal(m1));
  EXPECT_FALSE(is_orthogonal(m3));
  EXPECT_TRUE(is_orthonormal(m4));
  EXPECT_FALSE(is_orthonormal(m1));
  EXPECT_FALSE(is_orthonormal(m3));
  EXPECT_FALSE(is_uniformly_scaled(m1));
  EXPECT_TRUE(is_uniformly_scaled(m4));
  EXPECT_FALSE(is_zero(m4));
  EXPECT_TRUE(is_zero(m5));
  EXPECT_TRUE(is_negative(m4));
  EXPECT_FALSE(is_negative(m5));
  EXPECT_FALSE(is_negative(m6));
}

TEST(math_matrix, MatrixToNearestEuler)
{
  EulerXYZ eul1 = EulerXYZ(225.08542, -1.12485, -121.23738);
  Euler3 eul2 = Euler3(float3{4.06112, 100.561928, -18.9063}, EulerOrder::ZXY);

  float3x3 mat = {{0.808309, -0.578051, -0.111775},
                  {0.47251, 0.750174, -0.462572},
                  {0.351241, 0.321087, 0.879507}};

  EXPECT_V3_NEAR(float3(to_nearest_euler(mat, eul1)), float3(225.71, 0.112009, -120.001), 1e-3);
  EXPECT_V3_NEAR(float3(to_nearest_euler(mat, eul2)), float3(5.95631, 100.911, -19.5061), 1e-3);
}

TEST(math_matrix, MatrixMethods)
{
  float4x4 m = float4x4({0, 3, 0, 0}, {2, 0, 0, 0}, {0, 0, 2, 0}, {0, 1, 0, 1});
  auto expect_eul = EulerXYZ(0, 0, M_PI_2);
  auto expect_qt = Quaternion(0, -M_SQRT1_2, M_SQRT1_2, 0);
  float3 expect_scale = float3(3, 2, 2);
  float3 expect_location = float3(0, 1, 0);

  EXPECT_EQ(to_scale(m), expect_scale);

  float4 expect_sz = {3, 2, 2, M_SQRT2};
  float4 size;
  float4x4 m1 = normalize_and_get_size(m, size);
  EXPECT_TRUE(is_unit_scale(m1));
  EXPECT_V4_NEAR(size, expect_sz, 0.0002f);

  float4x4 m2 = normalize(m);
  EXPECT_TRUE(is_unit_scale(m2));

  EXPECT_V3_NEAR(float3(to_euler(m1)), float3(expect_eul), 0.0002f);
  EXPECT_V4_NEAR(float4(to_quaternion(m1)), float4(expect_qt), 0.0002f);

  EulerXYZ eul;
  Quaternion qt;
  float3 scale;
  to_rot_scale(float3x3(m), eul, scale);
  to_rot_scale(float3x3(m), qt, scale);
  EXPECT_V3_NEAR(scale, expect_scale, 0.00001f);
  EXPECT_V4_NEAR(float4(qt), float4(expect_qt), 0.0002f);
  EXPECT_V3_NEAR(float3(eul), float3(expect_eul), 0.0002f);

  float3 loc;
  to_loc_rot_scale(m, loc, eul, scale);
  to_loc_rot_scale(m, loc, qt, scale);
  EXPECT_V3_NEAR(scale, expect_scale, 0.00001f);
  EXPECT_V3_NEAR(loc, expect_location, 0.00001f);
  EXPECT_V4_NEAR(float4(qt), float4(expect_qt), 0.0002f);
  EXPECT_V3_NEAR(float3(eul), float3(expect_eul), 0.0002f);
}

TEST(math_matrix, MatrixToQuaternionLegacy)
{
  float3x3 mat = {{0.808309, -0.578051, -0.111775},
                  {0.47251, 0.750174, -0.462572},
                  {0.351241, 0.321087, 0.879507}};

  EXPECT_V4_NEAR(float4(to_quaternion_legacy(mat)),
                 float4(0.927091f, -0.211322f, 0.124857f, -0.283295f),
                 1e-5f);
}

TEST(math_matrix, MatrixTranspose)
{
  float4x4 m({1, 2, 3, 4}, {5, 6, 7, 8}, {9, 1, 2, 3}, {2, 5, 6, 7});
  float4x4 expect({1, 5, 9, 2}, {2, 6, 1, 5}, {3, 7, 2, 6}, {4, 8, 3, 7});
  EXPECT_EQ(transpose(m), expect);
}

TEST(math_matrix, MatrixInterpolationRegular)
{
  /* Test 4x4 matrix interpolation without singularity, i.e. without axis flip. */

  /* Transposed matrix, so that the code here is written in the same way as print_m4() outputs. */
  /* This matrix represents T=(0.1, 0.2, 0.3), R=(40, 50, 60) degrees, S=(0.7, 0.8, 0.9) */
  float4x4 m2 = transpose(float4x4({0.224976f, -0.333770f, 0.765074f, 0.100000f},
                                   {0.389669f, 0.647565f, 0.168130f, 0.200000f},
                                   {-0.536231f, 0.330541f, 0.443163f, 0.300000f},
                                   {0.000000f, 0.000000f, 0.000000f, 1.000000f}));
  float4x4 m1 = float4x4::identity();
  float4x4 result;
  const float epsilon = 1e-6;
  result = interpolate(m1, m2, 0.0f);
  EXPECT_M4_NEAR(result, m1, epsilon);
  result = interpolate(m1, m2, 1.0f);
  EXPECT_M4_NEAR(result, m2, epsilon);

  /* This matrix is based on the current implementation of the code, and isn't guaranteed to be
   * correct. It's just consistent with the current implementation. */
  float4x4 expect = transpose(float4x4({0.690643f, -0.253244f, 0.484996f, 0.050000f},
                                       {0.271924f, 0.852623f, 0.012348f, 0.100000f},
                                       {-0.414209f, 0.137484f, 0.816778f, 0.150000f},
                                       {0.000000f, 0.000000f, 0.000000f, 1.000000f}));
  result = interpolate(m1, m2, 0.5f);
  EXPECT_M4_NEAR(result, expect, epsilon);

  result = interpolate_fast(m1, m2, 0.5f);
  EXPECT_M4_NEAR(result, expect, epsilon);
}

TEST(math_matrix, MatrixInterpolationSingularity)
{
  /* A singularity means that there is an axis mirror in the rotation component of the matrix.
   * This is reflected in its negative determinant.
   *
   * The interpolation of 4x4 matrices performs linear interpolation on the translation component,
   * and then uses the 3x3 interpolation function to handle rotation and scale. As a result, this
   * test for a singularity in the rotation matrix only needs to test the 3x3 case. */

  /* Transposed matrix, so that the code here is written in the same way as print_m4() outputs. */
  /* This matrix represents R=(4, 5, 6) degrees, S=(-1, 1, 1) */
  float3x3 matrix_a = transpose(float3x3({-0.990737f, -0.098227f, 0.093759f},
                                         {-0.104131f, 0.992735f, -0.060286f},
                                         {0.087156f, 0.069491f, 0.993768f}));
  EXPECT_NEAR(-1.0f, determinant(matrix_a), 1e-6);

  /* This matrix represents R=(0, 0, 0), S=(-1, 1 1) */
  float3x3 matrix_b = transpose(
      float3x3({-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}));

  float3x3 result = interpolate(matrix_a, matrix_b, 0.0f);
  EXPECT_M3_NEAR(result, matrix_a, 1e-5);

  result = interpolate(matrix_a, matrix_b, 1.0f);
  EXPECT_M3_NEAR(result, matrix_b, 1e-5);

  result = interpolate(matrix_a, matrix_b, 0.5f);

  float3x3 expect = transpose(float3x3({-0.997681f, -0.049995f, 0.046186f},
                                       {-0.051473f, 0.998181f, -0.031385f},
                                       {0.044533f, 0.033689f, 0.998440f}));
  EXPECT_M3_NEAR(result, expect, 1e-5);

  result = interpolate_fast(matrix_a, matrix_b, 0.5f);
  EXPECT_M3_NEAR(result, expect, 1e-5);

  /* Interpolating between a matrix with and without axis flip can cause it to go through a zero
   * point. The determinant det(A) of a matrix represents the change in volume; interpolating
   * between matrices with det(A)=-1 and det(B)=1 will have to go through a point where
   * det(result)=0, so where the volume becomes zero. */
  float3x3 matrix_i = float3x3::identity();
  expect = float3x3::zero();
  result = interpolate(matrix_a, matrix_i, 0.5f);
  EXPECT_NEAR(0.0f, determinant(result), 1e-5);
  EXPECT_M3_NEAR(result, expect, 1e-5);
}

TEST(math_matrix, MatrixTransform)
{
  float3 expect, result;
  const float3 p(1, 2, 3);
  float4x4 m4 = from_loc_rot<float4x4>({10, 0, 0}, EulerXYZ(M_PI_2, M_PI_2, M_PI_2));
  float3x3 m3 = from_rotation<float3x3>(EulerXYZ(M_PI_2, M_PI_2, M_PI_2));
  float4x4 pers4 = projection::perspective(-0.1f, 0.1f, -0.1f, 0.1f, -0.1f, -1.0f);
  float3x3 pers3 = float3x3({1, 0, 0.1f}, {0, 1, 0.1f}, {0, 0.1f, 1});

  expect = {13, 2, -1};
  result = transform_point(m4, p);
  EXPECT_V3_NEAR(result, expect, 1e-2);

  expect = {3, 2, -1};
  result = transform_point(m3, p);
  EXPECT_V3_NEAR(result, expect, 1e-5);

  result = transform_direction(m4, p);
  EXPECT_V3_NEAR(result, expect, 1e-5);

  result = transform_direction(m3, p);
  EXPECT_V3_NEAR(result, expect, 1e-5);

  expect = {-0.333333, -0.666666, -1.14814};
  result = project_point(pers4, p);
  EXPECT_V3_NEAR(result, expect, 1e-5);

  float2 expect2 = {0.76923, 1.61538};
  float2 result2 = project_point(pers3, float2(p));
  EXPECT_V2_NEAR(result2, expect2, 1e-5);
}

TEST(math_matrix, MatrixProjection)
{
  using namespace math::projection;
  float4x4 expect;
  float4x4 ortho = orthographic(-0.2f, 0.3f, -0.2f, 0.4f, -0.2f, -0.5f);
  float4x4 pers1 = perspective(-0.2f, 0.3f, -0.2f, 0.4f, -0.2f, -0.5f);
  float4x4 pers2 = perspective_fov(
      math::atan(-0.2f), math::atan(0.3f), math::atan(-0.2f), math::atan(0.4f), -0.2f, -0.5f);

  expect = transpose(float4x4({4.0f, 0.0f, 0.0f, -0.2f},
                              {0.0f, 3.33333f, 0.0f, -0.333333f},
                              {0.0f, 0.0f, 6.66667f, -2.33333f},
                              {0.0f, 0.0f, 0.0f, 1.0f}));
  EXPECT_M4_NEAR(ortho, expect, 1e-5);

  expect = transpose(float4x4({-0.8f, 0.0f, 0.2f, 0.0f},
                              {0.0f, -0.666667f, 0.333333f, 0.0f},
                              {0.0f, 0.0f, -2.33333f, 0.666667f},
                              {0.0f, 0.0f, -1.0f, 0.0f}));
  EXPECT_M4_NEAR(pers1, expect, 1e-5);

  expect = transpose(float4x4({4.0f, 0.0f, 0.2f, 0.0f},
                              {0.0f, 3.33333f, 0.333333f, 0.0f},
                              {0.0f, 0.0f, -2.33333f, 0.666667f},
                              {0.0f, 0.0f, -1.0f, 0.0f}));
  EXPECT_M4_NEAR(pers2, expect, 1e-5);
}

}  // namespace blender::tests
