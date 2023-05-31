/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math.h"

#include "BLI_math_vector.hh"

namespace blender::tests {

TEST(math_vector, ClampVecWithFloats)
{
  const float min = 0.0f;
  const float max = 1.0f;

  float a[2] = {-1.0f, -1.0f};
  clamp_v2(a, min, max);
  EXPECT_FLOAT_EQ(0.0f, a[0]);
  EXPECT_FLOAT_EQ(0.0f, a[1]);

  float b[2] = {0.5f, 0.5f};
  clamp_v2(b, min, max);
  EXPECT_FLOAT_EQ(0.5f, b[0]);
  EXPECT_FLOAT_EQ(0.5f, b[1]);

  float c[2] = {2.0f, 2.0f};
  clamp_v2(c, min, max);
  EXPECT_FLOAT_EQ(1.0f, c[0]);
  EXPECT_FLOAT_EQ(1.0f, c[1]);
}

TEST(math_vector, ClampVecWithVecs)
{
  const float min[2] = {0.0f, 2.0f};
  const float max[2] = {1.0f, 3.0f};

  float a[2] = {-1.0f, -1.0f};
  clamp_v2_v2v2(a, min, max);
  EXPECT_FLOAT_EQ(0.0f, a[0]);
  EXPECT_FLOAT_EQ(2.0f, a[1]);

  float b[2] = {0.5f, 2.5f};
  clamp_v2_v2v2(b, min, max);
  EXPECT_FLOAT_EQ(0.5f, b[0]);
  EXPECT_FLOAT_EQ(2.5f, b[1]);

  float c[2] = {2.0f, 4.0f};
  clamp_v2_v2v2(c, min, max);
  EXPECT_FLOAT_EQ(1.0f, c[0]);
  EXPECT_FLOAT_EQ(3.0f, c[1]);
}

TEST(math_vector, test_invert_v3_safe)
{
  float v3_with_zeroes[3] = {0.0f, 2.0f, 3.0f};
  invert_v3_safe(v3_with_zeroes);
  EXPECT_FLOAT_EQ(0.0f, v3_with_zeroes[0]);
  EXPECT_FLOAT_EQ(0.5f, v3_with_zeroes[1]);
  EXPECT_FLOAT_EQ(0.33333333333f, v3_with_zeroes[2]);

  float v3_without_zeroes[3] = {1.0f, 2.0f, 3.0f};
  float inverted_unsafe[3] = {1.0f, 2.0f, 3.0f};
  invert_v3_safe(v3_without_zeroes);
  invert_v3(inverted_unsafe);

  EXPECT_FLOAT_EQ(inverted_unsafe[0], v3_without_zeroes[0]);
  EXPECT_FLOAT_EQ(inverted_unsafe[1], v3_without_zeroes[1]);
  EXPECT_FLOAT_EQ(inverted_unsafe[2], v3_without_zeroes[2]);
}

TEST(math_vector, Clamp)
{
  const int3 value(0, 100, -100);
  const int3 min(5, 40, -95);
  const int3 max(7, 45, 5);

  const int3 result = math::clamp(value, min, max);
  EXPECT_EQ(result.x, 5);
  EXPECT_EQ(result.y, 45);
  EXPECT_EQ(result.z, -95);

  const int3 result_2 = math::clamp(value, -50, 50);
  EXPECT_EQ(result_2.x, 0);
  EXPECT_EQ(result_2.y, 50);
  EXPECT_EQ(result_2.z, -50);
}

TEST(math_vector, InterpolateInt)
{
  const int3 a(0, -100, 50);
  const int3 b(0, 100, 100);
  const int3 result = math::interpolate(a, b, 0.75);
  EXPECT_EQ(result.x, 0);
  EXPECT_EQ(result.y, 50);
  EXPECT_EQ(result.z, 87);
}

TEST(math_vector, InterpolateFloat)
{
  const float3 a(40.0f, -100.0f, 50.0f);
  const float3 b(20.0f, 100.0f, 100.0f);
  const float3 result = math::interpolate(a, b, 0.5);
  EXPECT_FLOAT_EQ(result.x, 30.0f);
  EXPECT_FLOAT_EQ(result.y, 0.0f);
  EXPECT_FLOAT_EQ(result.z, 75.0f);
}

TEST(math_vector, CeilToMultiple)
{
  const int3 a(21, 16, 0);
  const int3 b(8, 16, 15);
  const int3 result = math::ceil_to_multiple(a, b);
  EXPECT_FLOAT_EQ(result.x, 24);
  EXPECT_FLOAT_EQ(result.y, 16);
  EXPECT_FLOAT_EQ(result.z, 0);
}

TEST(math_vector, DivideCeil)
{
  const int3 a(21, 16, 0);
  const int3 b(8, 16, 15);
  const int3 result = math::divide_ceil(a, b);
  EXPECT_FLOAT_EQ(result.x, 3);
  EXPECT_FLOAT_EQ(result.y, 1);
  EXPECT_FLOAT_EQ(result.z, 0);
}

TEST(math_vector, Sign)
{
  const int3 a(-21, 16, 0);
  const int3 result = math::sign(a);
  EXPECT_FLOAT_EQ(result.x, -1);
  EXPECT_FLOAT_EQ(result.y, 1);
  EXPECT_FLOAT_EQ(result.z, 0);
}

}  // namespace blender::tests
