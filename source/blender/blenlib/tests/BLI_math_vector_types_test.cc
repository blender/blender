/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_vector_types.hh"

namespace blender::tests {

using namespace blender::math;

TEST(math_vec_types, ScalarConstructorUnsigned)
{
  float2 u(5u);
  EXPECT_EQ(u[0], 5.0f);
  EXPECT_EQ(u[1], 5.0f);
}

TEST(math_vec_types, ScalarConstructorInt)
{
  float2 i(-5);
  EXPECT_EQ(i[0], -5.0f);
  EXPECT_EQ(i[1], -5.0f);
}

TEST(math_vec_types, ScalarConstructorFloat)
{
  float2 f(5.2f);
  EXPECT_FLOAT_EQ(f[0], 5.2f);
  EXPECT_FLOAT_EQ(f[1], 5.2f);
}

TEST(math_vec_types, ScalarConstructorDouble)
{
  float2 d(5.2);
  EXPECT_FLOAT_EQ(d[0], 5.2f);
  EXPECT_FLOAT_EQ(d[1], 5.2f);
}

TEST(math_vec_types, MultiScalarConstructorVec2)
{
  int2 i(5, -1);
  EXPECT_EQ(i[0], 5);
  EXPECT_EQ(i[1], -1);
}

TEST(math_vec_types, MultiScalarConstructorVec3)
{
  int3 i(5, -1, 6u);
  EXPECT_EQ(i[0], 5);
  EXPECT_EQ(i[1], -1);
  EXPECT_EQ(i[2], 6);
}

TEST(math_vec_types, MultiScalarConstructorVec4)
{
  int4 i(5, -1, 6u, 0);
  EXPECT_EQ(i[0], 5);
  EXPECT_EQ(i[1], -1);
  EXPECT_EQ(i[2], 6);
  EXPECT_EQ(i[3], 0);
}

TEST(math_vec_types, MixedScalarVectorConstructorVec3)
{
  float3 fl_v2(float2(5.5f), 1.8f);
  EXPECT_FLOAT_EQ(fl_v2[0], 5.5f);
  EXPECT_FLOAT_EQ(fl_v2[1], 5.5f);
  EXPECT_FLOAT_EQ(fl_v2[2], 1.8f);

  float3 v2_fl(1.8f, float2(5.5f));
  EXPECT_FLOAT_EQ(v2_fl[0], 1.8f);
  EXPECT_FLOAT_EQ(v2_fl[1], 5.5f);
  EXPECT_FLOAT_EQ(v2_fl[2], 5.5f);
}

TEST(math_vec_types, MixedScalarVectorConstructorVec4)
{
  int4 v2_fl_fl(float2(1), 2, 3);
  EXPECT_EQ(v2_fl_fl[0], 1);
  EXPECT_EQ(v2_fl_fl[1], 1);
  EXPECT_EQ(v2_fl_fl[2], 2);
  EXPECT_EQ(v2_fl_fl[3], 3);

  float4 fl_v2_fl(1, int2(2), 3);
  EXPECT_EQ(fl_v2_fl[0], 1);
  EXPECT_EQ(fl_v2_fl[1], 2);
  EXPECT_EQ(fl_v2_fl[2], 2);
  EXPECT_EQ(fl_v2_fl[3], 3);

  double4 fl_fl_v2(1, 2, double2(3));
  EXPECT_EQ(fl_fl_v2[0], 1);
  EXPECT_EQ(fl_fl_v2[1], 2);
  EXPECT_EQ(fl_fl_v2[2], 3);
  EXPECT_EQ(fl_fl_v2[3], 3);

  int4 v2_v2(float2(1), uint2(2));
  EXPECT_EQ(v2_v2[0], 1);
  EXPECT_EQ(v2_v2[1], 1);
  EXPECT_EQ(v2_v2[2], 2);
  EXPECT_EQ(v2_v2[3], 2);

  float4 v3_fl(uint3(1), 2);
  EXPECT_EQ(v3_fl[0], 1);
  EXPECT_EQ(v3_fl[1], 1);
  EXPECT_EQ(v3_fl[2], 1);
  EXPECT_EQ(v3_fl[3], 2);

  uint4 fl_v3(1, float3(2));
  EXPECT_EQ(fl_v3[0], 1);
  EXPECT_EQ(fl_v3[1], 2);
  EXPECT_EQ(fl_v3[2], 2);
  EXPECT_EQ(fl_v3[3], 2);
}

TEST(math_vec_types, ComponentMasking)
{
  int4 i(0, 1, 2, 3);
  float2 f2 = float2(i);
  EXPECT_EQ(f2[0], 0.0f);
  EXPECT_EQ(f2[1], 1.0f);
}

TEST(math_vec_types, PointerConversion)
{
  float array[3] = {1.0f, 2.0f, 3.0f};
  float3 farray(array);
  EXPECT_EQ(farray[0], 1.0f);
  EXPECT_EQ(farray[1], 2.0f);
  EXPECT_EQ(farray[2], 3.0f);
}

TEST(math_vec_types, PointerArrayConversion)
{
  float array[1][3] = {{1.0f, 2.0f, 3.0f}};
  float(*ptr)[3] = array;
  float3 fptr(ptr);
  EXPECT_EQ(fptr[0], 1.0f);
  EXPECT_EQ(fptr[1], 2.0f);
  EXPECT_EQ(fptr[2], 3.0f);
}

TEST(math_vec_types, VectorTypeConversion)
{
  double2 d(int2(float2(5.75f, -1.57f)));
  EXPECT_EQ(d[0], 5.0);
  EXPECT_EQ(d[1], -1.0);
}

TEST(math_vec_types, Add)
{
  float2 result = float2(1.0f, 2.0f) + float2(0.5f, 2.0f);
  EXPECT_FLOAT_EQ(result.x, 1.5f);
  EXPECT_FLOAT_EQ(result.y, 4.0f);

  float2 result2 = float2(1.0f, 2.0f);
  result2 += float2(0.5f, 2.0f);
  EXPECT_FLOAT_EQ(result2.x, 1.5f);
  EXPECT_FLOAT_EQ(result2.y, 4.0f);
}

TEST(math_vec_types, AddFloatByVector)
{
  float2 result = float2(0.5f, 2.0f) + 2.0f;
  EXPECT_FLOAT_EQ(result.x, 2.5f);
  EXPECT_FLOAT_EQ(result.y, 4.0f);

  float2 result2 = 2.0f + float2(0.5f, 2.0f);
  EXPECT_FLOAT_EQ(result2.x, 2.5f);
  EXPECT_FLOAT_EQ(result2.y, 4.0f);

  float2 result3 = float2(0.5f, 2.0f);
  result3 += 2.0f;
  EXPECT_FLOAT_EQ(result3.x, 2.5f);
  EXPECT_FLOAT_EQ(result3.y, 4.0f);
}

TEST(math_vec_types, Sub)
{
  float2 result = float2(1.0f, 2.0f) - float2(0.5f, 2.0f);
  EXPECT_FLOAT_EQ(result.x, 0.5f);
  EXPECT_FLOAT_EQ(result.y, 0.0f);

  float2 result2 = float2(1.0f, 2.0f);
  result2 -= float2(0.5f, 2.0f);
  EXPECT_FLOAT_EQ(result2.x, 0.5f);
  EXPECT_FLOAT_EQ(result2.y, 0.0f);

  float2 result3 = -float2(1.0f, 2.0f);
  EXPECT_FLOAT_EQ(result3.x, -1.0f);
  EXPECT_FLOAT_EQ(result3.y, -2.0f);
}

TEST(math_vec_types, SubFloatByVector)
{
  float2 result = float2(0.5f, 2.0f) - 2.0f;
  EXPECT_FLOAT_EQ(result.x, -1.5f);
  EXPECT_FLOAT_EQ(result.y, 0.0f);

  float2 result2 = 2.0f - float2(0.5f, 2.0f);
  EXPECT_FLOAT_EQ(result2.x, 1.5f);
  EXPECT_FLOAT_EQ(result2.y, 0.0f);

  float2 result3 = float2(0.5f, 2.0f);
  result3 -= 2.0f;
  EXPECT_FLOAT_EQ(result3.x, -1.5f);
  EXPECT_FLOAT_EQ(result3.y, 0.0f);
}

TEST(math_vec_types, Mul)
{
  float2 result = float2(1.0f, 2.0f) * float2(0.5f, 2.0f);
  EXPECT_FLOAT_EQ(result.x, 0.5f);
  EXPECT_FLOAT_EQ(result.y, 4.0f);

  float2 result2 = float2(1.0f, 2.0f);
  result2 *= float2(0.5f, 2.0f);
  EXPECT_FLOAT_EQ(result2.x, 0.5f);
  EXPECT_FLOAT_EQ(result2.y, 4.0f);
}

TEST(math_vec_types, MulFloatByVector)
{
  float2 result = float2(0.5f, 2.0f) * 2.0f;
  EXPECT_FLOAT_EQ(result.x, 1.0f);
  EXPECT_FLOAT_EQ(result.y, 4.0f);

  float2 result2 = 2.0f * float2(0.5f, 2.0f);
  EXPECT_FLOAT_EQ(result2.x, 1.0f);
  EXPECT_FLOAT_EQ(result2.y, 4.0f);

  float2 result3 = float2(0.5f, 2.0f);
  result3 *= 2.0f;
  EXPECT_FLOAT_EQ(result3.x, 1.0f);
  EXPECT_FLOAT_EQ(result3.y, 4.0f);
}

TEST(math_vec_types, Divide)
{
  float2 a(1.0f, 2.0f);
  float2 b(0.5f, 2.0f);
  float2 result = a / b;
  EXPECT_FLOAT_EQ(result.x, 2.0f);
  EXPECT_FLOAT_EQ(result.y, 1.0f);
}

TEST(math_vec_types, DivideFloatByVector)
{
  float a = 2.0f;
  float2 b(0.5f, 2.0f);
  float2 result = a / b;
  EXPECT_FLOAT_EQ(result.x, 4.0f);
  EXPECT_FLOAT_EQ(result.y, 1.0f);
}

TEST(math_vec_types, DivideFloatByVectorSmall)
{
  float2 result = 2.0f / float2(2.0f);
  EXPECT_FLOAT_EQ(result.x, 1.0f);
  EXPECT_FLOAT_EQ(result.y, 1.0f);
}

TEST(math_vec_types, SwizzleReinterpret)
{
  const float2 v01(0, 1);
  const float2 v12(1, 2);
  const float2 v23(2, 3);
  const float3 v012(0, 1, 2);
  const float3 v123(1, 2, 3);
  const float4 v0123(0, 1, 2, 3);
  /* Identity. */
  EXPECT_EQ(v01.xy(), v01);
  EXPECT_EQ(v012.xyz(), v012);
  EXPECT_EQ(v0123.xyzw(), v0123);
  /* Masking. */
  EXPECT_EQ(v012.xy(), v01);
  EXPECT_EQ(v0123.xyz(), v012);
  /* Offset. */
  EXPECT_EQ(v0123.yz(), v12);
  EXPECT_EQ(v0123.zw(), v23);
  EXPECT_EQ(v0123.yzw(), v123);
}

}  // namespace blender::tests
