/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"

namespace blender::tests {

using namespace blender::math;

TEST(math_matrix_types, DefaultConstructor)
{
  float2x2 m{};
  EXPECT_EQ(m[0][0], 0.0f);
  EXPECT_EQ(m[1][1], 0.0f);
  EXPECT_EQ(m[0][1], 0.0f);
  EXPECT_EQ(m[1][0], 0.0f);
}

TEST(math_matrix_types, StaticConstructor)
{
  float2x2 m = float2x2::identity();
  EXPECT_EQ(m[0][0], 1.0f);
  EXPECT_EQ(m[1][1], 1.0f);
  EXPECT_EQ(m[0][1], 0.0f);
  EXPECT_EQ(m[1][0], 0.0f);

  m = float2x2::zero();
  EXPECT_EQ(m[0][0], 0.0f);
  EXPECT_EQ(m[1][1], 0.0f);
  EXPECT_EQ(m[0][1], 0.0f);
  EXPECT_EQ(m[1][0], 0.0f);

  m = float2x2::diagonal(2);
  EXPECT_EQ(m[0][0], 2.0f);
  EXPECT_EQ(m[1][1], 2.0f);
  EXPECT_EQ(m[0][1], 0.0f);
  EXPECT_EQ(m[1][0], 0.0f);

  m = float2x2::all(1);
  EXPECT_EQ(m[0][0], 1.0f);
  EXPECT_EQ(m[1][1], 1.0f);
  EXPECT_EQ(m[0][1], 1.0f);
  EXPECT_EQ(m[1][0], 1.0f);
}

TEST(math_matrix_types, VectorConstructor)
{
  float3x2 m({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f});
  EXPECT_EQ(m[0][0], 1.0f);
  EXPECT_EQ(m[0][1], 2.0f);
  EXPECT_EQ(m[1][0], 3.0f);
  EXPECT_EQ(m[1][1], 4.0f);
  EXPECT_EQ(m[2][0], 5.0f);
  EXPECT_EQ(m[2][1], 6.0f);
}

TEST(math_matrix_types, SmallerMatrixConstructor)
{
  float2x2 m2({1.0f, 2.0f}, {3.0f, 4.0f});
  float3x3 m3(m2);
  EXPECT_EQ(m3[0][0], 1.0f);
  EXPECT_EQ(m3[0][1], 2.0f);
  EXPECT_EQ(m3[0][2], 0.0f);
  EXPECT_EQ(m3[1][0], 3.0f);
  EXPECT_EQ(m3[1][1], 4.0f);
  EXPECT_EQ(m3[1][2], 0.0f);
  EXPECT_EQ(m3[2][0], 0.0f);
  EXPECT_EQ(m3[2][1], 0.0f);
  EXPECT_EQ(m3[2][2], 1.0f);
}

TEST(math_matrix_types, ComponentMasking)
{
  float3x3 m3({1.1f, 1.2f, 1.3f}, {2.1f, 2.2f, 2.3f}, {3.1f, 3.2f, 3.3f});
  float2x2 m2(m3);
  EXPECT_EQ(m2[0][0], 1.1f);
  EXPECT_EQ(m2[0][1], 1.2f);
  EXPECT_EQ(m2[1][0], 2.1f);
  EXPECT_EQ(m2[1][1], 2.2f);
}

TEST(math_matrix_types, PointerConversion)
{
  float array[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  float2x2 m2(array);
  EXPECT_EQ(m2[0][0], 1.0f);
  EXPECT_EQ(m2[0][1], 2.0f);
  EXPECT_EQ(m2[1][0], 3.0f);
  EXPECT_EQ(m2[1][1], 4.0f);
}

TEST(math_matrix_types, TypeConversion)
{
  float3x2 m(double3x2({1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f}));
  EXPECT_EQ(m[0][0], 1.0f);
  EXPECT_EQ(m[0][1], 2.0f);
  EXPECT_EQ(m[1][0], 3.0f);
  EXPECT_EQ(m[1][1], 4.0f);
  EXPECT_EQ(m[2][0], 5.0f);
  EXPECT_EQ(m[2][1], 6.0f);

  double3x2 d(m);
  EXPECT_EQ(d[0][0], 1.0f);
  EXPECT_EQ(d[0][1], 2.0f);
  EXPECT_EQ(d[1][0], 3.0f);
  EXPECT_EQ(d[1][1], 4.0f);
  EXPECT_EQ(d[2][0], 5.0f);
  EXPECT_EQ(d[2][1], 6.0f);
}

TEST(math_matrix_types, PointerArrayConversion)
{
  float array[2][2] = {{1.0f, 2.0f}, {3.0f, 4.0f}};
  float (*ptr)[2] = array;
  float2x2 m2(ptr);
  EXPECT_EQ(m2[0][0], 1.0f);
  EXPECT_EQ(m2[0][1], 2.0f);
  EXPECT_EQ(m2[1][0], 3.0f);
  EXPECT_EQ(m2[1][1], 4.0f);
}

TEST(math_matrix_types, ComponentAccess)
{
  float3x3 m3({1.1f, 1.2f, 1.3f}, {2.1f, 2.2f, 2.3f}, {3.1f, 3.2f, 3.3f});
  EXPECT_EQ(m3.x.x, 1.1f);
  EXPECT_EQ(m3.x.y, 1.2f);
  EXPECT_EQ(m3.y.x, 2.1f);
  EXPECT_EQ(m3.y.y, 2.2f);
}

TEST(math_matrix_types, AddOperator)
{
  float3x3 m3({1.1f, 1.2f, 1.3f}, {2.1f, 2.2f, 2.3f}, {3.1f, 3.2f, 3.3f});

  m3 = m3 + float3x3::diagonal(2);
  EXPECT_EQ(m3[0][0], 3.1f);
  EXPECT_EQ(m3[0][2], 1.3f);
  EXPECT_EQ(m3[2][0], 3.1f);
  EXPECT_EQ(m3[2][2], 5.3f);

  m3 += float3x3::diagonal(-1.0f);
  EXPECT_EQ(m3[0][0], 2.1f);
  EXPECT_EQ(m3[0][2], 1.3f);
  EXPECT_EQ(m3[2][0], 3.1f);
  EXPECT_EQ(m3[2][2], 4.3f);

  m3 += 1.0f;
  EXPECT_EQ(m3[0][0], 3.1f);
  EXPECT_EQ(m3[0][2], 2.3f);
  EXPECT_EQ(m3[2][0], 4.1f);
  EXPECT_EQ(m3[2][2], 5.3f);

  m3 = m3 + 1.0f;
  EXPECT_EQ(m3[0][0], 4.1f);
  EXPECT_EQ(m3[0][2], 3.3f);
  EXPECT_EQ(m3[2][0], 5.1f);
  EXPECT_EQ(m3[2][2], 6.3f);

  m3 = 1.0f + m3;
  EXPECT_EQ(m3[0][0], 5.1f);
  EXPECT_EQ(m3[0][2], 4.3f);
  EXPECT_EQ(m3[2][0], 6.1f);
  EXPECT_EQ(m3[2][2], 7.3f);
}

TEST(math_matrix_types, SubtractOperator)
{
  float3x3 m3({10.0f, 10.2f, 10.3f}, {20.1f, 20.2f, 20.3f}, {30.1f, 30.2f, 30.3f});

  m3 = m3 - float3x3::diagonal(2);
  EXPECT_EQ(m3[0][0], 8.0f);
  EXPECT_EQ(m3[0][2], 10.3f);
  EXPECT_EQ(m3[2][0], 30.1f);
  EXPECT_EQ(m3[2][2], 28.3f);

  m3 -= float3x3::diagonal(-1.0f);
  EXPECT_EQ(m3[0][0], 9.0f);
  EXPECT_EQ(m3[0][2], 10.3f);
  EXPECT_EQ(m3[2][0], 30.1f);
  EXPECT_EQ(m3[2][2], 29.3f);

  m3 -= 1.0f;
  EXPECT_EQ(m3[0][0], 8.0f);
  EXPECT_EQ(m3[0][2], 9.3f);
  EXPECT_EQ(m3[2][0], 29.1f);
  EXPECT_EQ(m3[2][2], 28.3f);

  m3 = m3 - 1.0f;
  EXPECT_EQ(m3[0][0], 7.0f);
  EXPECT_EQ(m3[0][2], 8.3f);
  EXPECT_EQ(m3[2][0], 28.1f);
  EXPECT_EQ(m3[2][2], 27.3f);

  m3 = 1.0f - m3;
  EXPECT_EQ(m3[0][0], -6.0f);
  EXPECT_EQ(m3[0][2], -7.3f);
  EXPECT_EQ(m3[2][0], -27.1f);
  EXPECT_EQ(m3[2][2], -26.3f);
}

TEST(math_matrix_types, MultiplyOperator)
{
  float3x3 m3(float3(1.0f), float3(2.0f), float3(2.0f));

  m3 = m3 * 2;
  EXPECT_EQ(m3[0][0], 2.0f);
  EXPECT_EQ(m3[2][2], 4.0f);

  m3 = 2 * m3;
  EXPECT_EQ(m3[0][0], 4.0f);
  EXPECT_EQ(m3[2][2], 8.0f);

  m3 *= 2;
  EXPECT_EQ(m3[0][0], 8.0f);
  EXPECT_EQ(m3[2][2], 16.0f);
}

TEST(math_matrix_types, MatrixMultiplyOperator)
{
  float2x2 a(float2(1, 2), float2(3, 4));
  float2x2 b(float2(5, 6), float2(7, 8));

  float2x2 result = a * b;
  EXPECT_EQ(result[0][0], 23);
  EXPECT_EQ(result[0][1], 34);
  EXPECT_EQ(result[1][0], 31);
  EXPECT_EQ(result[1][1], 46);

  result = a;
  result *= b;
  EXPECT_EQ(result[0][0], 23);
  EXPECT_EQ(result[0][1], 34);
  EXPECT_EQ(result[1][0], 31);
  EXPECT_EQ(result[1][1], 46);

  /* Test SSE2 implementation. */
  float4x4 result2 = float4x4::diagonal(2) * float4x4::diagonal(6);
  EXPECT_EQ(result2, float4x4::diagonal(12));

  float3x3 result3 = float3x3::diagonal(2) * float3x3::diagonal(6);
  EXPECT_EQ(result3, float3x3::diagonal(12));

  /* Non square matrices. */
  float3x2 a4(float2(1, 2), float2(3, 4), float2(5, 6));
  float2x3 b4(float3(11, 7, 5), float3(13, 11, 17));

  float2x2 expect4(float2(57, 80), float2(131, 172));

  float2x2 result4 = a4 * b4;
  EXPECT_EQ(result4[0][0], expect4[0][0]);
  EXPECT_EQ(result4[0][1], expect4[0][1]);
  EXPECT_EQ(result4[1][0], expect4[1][0]);
  EXPECT_EQ(result4[1][1], expect4[1][1]);

  float3x4 a5(float4(1), float4(3), float4(5));
  float2x3 b5(float3(11, 7, 5), float3(13, 11, 17));

  float2x4 expect5(float4(57), float4(131));

  float2x4 result5 = a5 * b5;
  EXPECT_EQ(result5[0][0], expect5[0][0]);
  EXPECT_EQ(result5[0][1], expect5[0][1]);
  EXPECT_EQ(result5[0][2], expect5[0][2]);
  EXPECT_EQ(result5[0][3], expect5[0][3]);
  EXPECT_EQ(result5[1][0], expect5[1][0]);
  EXPECT_EQ(result5[1][1], expect5[1][1]);
  EXPECT_EQ(result5[1][2], expect5[1][2]);
  EXPECT_EQ(result5[1][3], expect5[1][3]);
}

TEST(math_matrix_types, VectorMultiplyOperator)
{
  float3x2 mat(float2(1, 2), float2(3, 4), float2(5, 6));

  float2 result = mat * float3(7, 8, 9);
  EXPECT_EQ(result[0], 76);
  EXPECT_EQ(result[1], 100);

  float3 result2 = float2(2, 3) * mat;
  EXPECT_EQ(result2[0], 8);
  EXPECT_EQ(result2[1], 18);
  EXPECT_EQ(result2[2], 28);
}

TEST(math_matrix_types, ViewConstructor)
{
  float4x4 mat = float4x4(
      float4(1, 2, 3, 4), float4(5, 6, 7, 8), float4(9, 10, 11, 12), float4(13, 14, 15, 16));

  auto view = mat.view<2, 2, 1, 1>();
  EXPECT_EQ(view[0][0], 6);
  EXPECT_EQ(view[0][1], 7);
  EXPECT_EQ(view[1][0], 10);
  EXPECT_EQ(view[1][1], 11);

  float2x2 center = view;
  EXPECT_EQ(center[0][0], 6);
  EXPECT_EQ(center[0][1], 7);
  EXPECT_EQ(center[1][0], 10);
  EXPECT_EQ(center[1][1], 11);
}

TEST(math_matrix_types, ViewFromCstyleMatrix)
{
  float c_style_mat[4][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};
  float4x4_view c_mat_view = float4x4_view(c_style_mat);

  float4x4_mutableview c_mat_mutable_view = float4x4_mutableview(c_style_mat);

  float4x4 expect = float4x4({2, 4, 6, 8}, {10, 12, 14, 16}, {18, 20, 22, 24}, {26, 28, 30, 32});

  float4x4 mat = float4x4::diagonal(2.0f) * c_mat_view;
  EXPECT_M4_NEAR(expect, mat, 1e-8f);

  c_mat_mutable_view *= float4x4::diagonal(2.0f);
  EXPECT_M4_NEAR(expect, c_mat_mutable_view, 1e-8f);
}

TEST(math_matrix_types, ViewAssignment)
{
  float4x4 mat = float4x4(
      float4(1, 2, 3, 4), float4(5, 6, 7, 8), float4(9, 10, 11, 12), float4(13, 14, 15, 16));

  mat.view<2, 2, 1, 1>() = float2x2({-1, -2}, {-3, -4});

  float4x4 expect = float4x4({1, 2, 3, 4}, {5, -1, -2, 8}, {9, -3, -4, 12}, {13, 14, 15, 16});
  EXPECT_M4_NEAR(expect, mat, 1e-8f);

  /* Test view-view assignment. */
  mat.view<2, 2, 2, 2>() = mat.view<2, 2, 0, 0>();
  float4x4 expect2 = float4x4({1, 2, 3, 4}, {5, -1, -2, 8}, {9, -3, 1, 2}, {13, 14, 5, -1});
  EXPECT_M4_NEAR(expect2, mat, 1e-8f);

  mat.view<2, 2, 0, 0>() = mat.view<2, 2, 1, 1>();
  float4x4 expect3 = float4x4({-1, -2, 3, 4}, {-3, 1, -2, 8}, {9, -3, 1, 2}, {13, 14, 5, -1});
  EXPECT_M4_NEAR(expect3, mat, 1e-8f);

  /* Should fail to compile. */
  // const float4x4 &mat_const = mat;
  // mat.view<2, 2, 2, 2>() = mat_const.view<2, 2, 0, 0>();

  /* Should fail to run. */
  // mat.view<2, 2, 1, 1>() = mat.view<2, 2>();
}

TEST(math_matrix_types, ViewScalarOperators)
{
  float4x4 mat = float4x4({1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16});

  auto view = mat.view<2, 2, 1, 1>();
  EXPECT_EQ(view[0][0], 6);
  EXPECT_EQ(view[0][1], 7);
  EXPECT_EQ(view[1][0], 10);
  EXPECT_EQ(view[1][1], 11);

  view += 1;
  EXPECT_EQ(view[0][0], 7);
  EXPECT_EQ(view[0][1], 8);
  EXPECT_EQ(view[1][0], 11);
  EXPECT_EQ(view[1][1], 12);

  view -= 2;
  EXPECT_EQ(view[0][0], 5);
  EXPECT_EQ(view[0][1], 6);
  EXPECT_EQ(view[1][0], 9);
  EXPECT_EQ(view[1][1], 10);

  view *= 4;
  EXPECT_EQ(view[0][0], 20);
  EXPECT_EQ(view[0][1], 24);
  EXPECT_EQ(view[1][0], 36);
  EXPECT_EQ(view[1][1], 40);

  /* Since we modified the view, we expect the source to have changed. */
  float4x4 expect = float4x4({1, 2, 3, 4}, {5, 20, 24, 8}, {9, 36, 40, 12}, {13, 14, 15, 16});
  EXPECT_M4_NEAR(expect, mat, 1e-8f);

  view = -view;
  EXPECT_EQ(view[0][0], -20);
  EXPECT_EQ(view[0][1], -24);
  EXPECT_EQ(view[1][0], -36);
  EXPECT_EQ(view[1][1], -40);
}

TEST(math_matrix_types, ViewMatrixMultiplyOperator)
{
  float4x4 mat = float4x4({1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16});
  auto view = mat.view<2, 2, 1, 1>();
  view = float2x2({1, 2}, {3, 4});

  float2x2 result = view * float2x2({5, 6}, {7, 8});
  EXPECT_EQ(result[0][0], 23);
  EXPECT_EQ(result[0][1], 34);
  EXPECT_EQ(result[1][0], 31);
  EXPECT_EQ(result[1][1], 46);

  view *= float2x2({5, 6}, {7, 8});
  EXPECT_EQ(view[0][0], 23);
  EXPECT_EQ(view[0][1], 34);
  EXPECT_EQ(view[1][0], 31);
  EXPECT_EQ(view[1][1], 46);
}

TEST(math_matrix_types, ViewVectorMultiplyOperator)
{
  float4x4 mat = float4x4({1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16});
  auto view = mat.view<2, 3, 1, 1>();

  float3 result = view * float2(4, 5);
  EXPECT_EQ(result[0], 74);
  EXPECT_EQ(result[1], 83);
  EXPECT_EQ(result[2], 92);

  float2 result2 = float3(1, 2, 3) * view;
  EXPECT_EQ(result2[0], 44);
  EXPECT_EQ(result2[1], 68);
}

TEST(math_matrix_types, ViewMatrixNormalize)
{
  float4x4 mat = float4x4({1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16});
  mat.view<3, 3>() = normalize(mat.view<3, 3>());

  float4x4 expect = float4x4({0.267261236, 0.534522473, 0.80178368, 4},
                             {0.476731300, 0.572077572, 0.66742378, 8},
                             {0.517891824, 0.575435340, 0.63297885, 12},
                             {13, 14, 15, 16});
  EXPECT_M4_NEAR(expect, mat, 1e-8f);
}

}  // namespace blender::tests
