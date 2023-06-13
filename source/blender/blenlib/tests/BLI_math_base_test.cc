/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"

namespace blender::tests {

/* In tests below, when we are using -1.0f as max_diff value, we actually turn the function into a
 * pure-ULP one. */

/* Put this here, since we cannot use BLI_assert() in inline math files it seems... */
TEST(math_base, CompareFFRelativeValid)
{
  EXPECT_TRUE(sizeof(float) == sizeof(int));
}

TEST(math_base, CompareFFRelativeNormal)
{
  float f1 = 1.99999988f; /* *(float *)&(*(int *)&f2 - 1) */
  float f2 = 2.00000000f;
  float f3 = 2.00000048f; /* *(float *)&(*(int *)&f2 + 2) */
  float f4 = 2.10000000f; /* *(float *)&(*(int *)&f2 + 419430) */

  const float max_diff = FLT_EPSILON * 0.1f;

  EXPECT_TRUE(compare_ff_relative(f1, f2, max_diff, 1));
  EXPECT_TRUE(compare_ff_relative(f2, f1, max_diff, 1));

  EXPECT_TRUE(compare_ff_relative(f3, f2, max_diff, 2));
  EXPECT_TRUE(compare_ff_relative(f2, f3, max_diff, 2));

  EXPECT_FALSE(compare_ff_relative(f3, f2, max_diff, 1));
  EXPECT_FALSE(compare_ff_relative(f2, f3, max_diff, 1));

  EXPECT_FALSE(compare_ff_relative(f3, f2, -1.0f, 1));
  EXPECT_FALSE(compare_ff_relative(f2, f3, -1.0f, 1));

  EXPECT_TRUE(compare_ff_relative(f3, f2, -1.0f, 2));
  EXPECT_TRUE(compare_ff_relative(f2, f3, -1.0f, 2));

  EXPECT_FALSE(compare_ff_relative(f4, f2, max_diff, 64));
  EXPECT_FALSE(compare_ff_relative(f2, f4, max_diff, 64));

  EXPECT_TRUE(compare_ff_relative(f1, f3, max_diff, 64));
  EXPECT_TRUE(compare_ff_relative(f3, f1, max_diff, 64));
}

TEST(math_base, CompareFFRelativeZero)
{
  float f0 = 0.0f;
  float f1 = 4.2038954e-045f; /* *(float *)&(*(int *)&f0 + 3) */

  float fn0 = -0.0f;
  float fn1 = -2.8025969e-045f; /* *(float *)&(*(int *)&fn0 - 2) */

  const float max_diff = FLT_EPSILON * 0.1f;

  EXPECT_TRUE(compare_ff_relative(f0, f1, -1.0f, 3));
  EXPECT_TRUE(compare_ff_relative(f1, f0, -1.0f, 3));

  EXPECT_FALSE(compare_ff_relative(f0, f1, -1.0f, 1));
  EXPECT_FALSE(compare_ff_relative(f1, f0, -1.0f, 1));

  EXPECT_TRUE(compare_ff_relative(fn0, fn1, -1.0f, 8));
  EXPECT_TRUE(compare_ff_relative(fn1, fn0, -1.0f, 8));

  EXPECT_TRUE(compare_ff_relative(f0, f1, max_diff, 1));
  EXPECT_TRUE(compare_ff_relative(f1, f0, max_diff, 1));

  EXPECT_TRUE(compare_ff_relative(fn0, f0, max_diff, 1));
  EXPECT_TRUE(compare_ff_relative(f0, fn0, max_diff, 1));

  EXPECT_TRUE(compare_ff_relative(f0, fn1, max_diff, 1));
  EXPECT_TRUE(compare_ff_relative(fn1, f0, max_diff, 1));

  /* NOTE: in theory, this should return false, since 0.0f  and -0.0f have 0x80000000 diff,
   *       but overflow in subtraction seems to break something here
   *       (abs(*(int *)&fn0 - *(int *)&f0) == 0x80000000 == fn0), probably because int32 cannot
   * hold this abs value. this is yet another illustration of why one shall never use (near-)zero
   * floats in pure-ULP comparison. */
  //  EXPECT_FALSE(compare_ff_relative(fn0, f0, -1.0f, 1024));
  //  EXPECT_FALSE(compare_ff_relative(f0, fn0, -1.0f, 1024));

  EXPECT_FALSE(compare_ff_relative(fn0, f1, -1.0f, 1024));
  EXPECT_FALSE(compare_ff_relative(f1, fn0, -1.0f, 1024));
}

TEST(math_base, Log2FloorU)
{
  EXPECT_EQ(log2_floor_u(0), 0);
  EXPECT_EQ(log2_floor_u(1), 0);
  EXPECT_EQ(log2_floor_u(2), 1);
  EXPECT_EQ(log2_floor_u(3), 1);
  EXPECT_EQ(log2_floor_u(4), 2);
  EXPECT_EQ(log2_floor_u(5), 2);
  EXPECT_EQ(log2_floor_u(6), 2);
  EXPECT_EQ(log2_floor_u(7), 2);
  EXPECT_EQ(log2_floor_u(8), 3);
  EXPECT_EQ(log2_floor_u(9), 3);
  EXPECT_EQ(log2_floor_u(123456), 16);
}

TEST(math_base, Log2CeilU)
{
  EXPECT_EQ(log2_ceil_u(0), 0);
  EXPECT_EQ(log2_ceil_u(1), 0);
  EXPECT_EQ(log2_ceil_u(2), 1);
  EXPECT_EQ(log2_ceil_u(3), 2);
  EXPECT_EQ(log2_ceil_u(4), 2);
  EXPECT_EQ(log2_ceil_u(5), 3);
  EXPECT_EQ(log2_ceil_u(6), 3);
  EXPECT_EQ(log2_ceil_u(7), 3);
  EXPECT_EQ(log2_ceil_u(8), 3);
  EXPECT_EQ(log2_ceil_u(9), 4);
  EXPECT_EQ(log2_ceil_u(123456), 17);
}

TEST(math_base, CeilPowerOf10)
{
  EXPECT_EQ(ceil_power_of_10(0), 0);
  EXPECT_EQ(ceil_power_of_10(1), 1);
  EXPECT_EQ(ceil_power_of_10(1e-6f), 1e-6f);
  EXPECT_NEAR(ceil_power_of_10(100.1f), 1000.0f, 1e-4f);
  EXPECT_NEAR(ceil_power_of_10(99.9f), 100.0f, 1e-4f);
}

TEST(math_base, FloorPowerOf10)
{
  EXPECT_EQ(floor_power_of_10(0), 0);
  EXPECT_EQ(floor_power_of_10(1), 1);
  EXPECT_EQ(floor_power_of_10(1e-6f), 1e-6f);
  EXPECT_NEAR(floor_power_of_10(100.1f), 100.0f, 1e-4f);
  EXPECT_NEAR(floor_power_of_10(99.9f), 10.0f, 1e-4f);
}

TEST(math_base, MinVectorAndFloat)
{
  EXPECT_EQ(math::min(1.0f, 2.0f), 1.0f);
}

TEST(math_base, ClampInt)
{
  EXPECT_EQ(math::clamp(111, -50, 101), 101);
}

TEST(math_base, Midpoint)
{
  EXPECT_NEAR(math::midpoint(100.0f, 200.0f), 150.0f, 1e-4f);
}

TEST(math_base, InterpolateInt)
{
  EXPECT_EQ(math::interpolate(100, 200, 0.4f), 140);
}

TEST(math_base, ModFPositive)
{
  EXPECT_FLOAT_EQ(mod_f_positive(3.27f, 1.57f), 0.12999988f);
  EXPECT_FLOAT_EQ(mod_f_positive(327.f, 47.f), 45.f);
  EXPECT_FLOAT_EQ(mod_f_positive(-0.1f, 1.0f), 0.9f);
}

}  // namespace blender::tests
