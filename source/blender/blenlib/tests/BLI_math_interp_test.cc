/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_color.hh"
#include "BLI_math_interp.hh"

using namespace blender;
using namespace blender::math;

static constexpr float float_tolerance = 0.00005f;
static constexpr int image_width = 3;
static constexpr int image_height = 3;
static constexpr uchar image_char[image_height][image_width][4] = {
    {{255, 254, 217, 216}, {230, 230, 230, 230}, {240, 160, 90, 20}},
    {{0, 1, 2, 3}, {62, 72, 82, 92}, {126, 127, 128, 129}},
    {{1, 2, 3, 4}, {73, 108, 153, 251}, {128, 129, 130, 131}},
};
static constexpr float image_fl[image_height][image_width][4] = {
    {{255, 254, 217, 216}, {230, 230, 230, 230}, {240, 160, 90, 20}},
    {{0, 1, 2, 3}, {62, 72, 82, 92}, {126, 127, 128, 129}},
    {{1, 2, 3, 4}, {73, 108, 153, 251}, {128, 129, 130, 131}},
};

TEST(math_interp, NearestCharExactSamples)
{
  uchar4 res;
  uchar4 exp1 = {73, 108, 153, 251};
  res = interpolate_nearest_border_byte(image_char[0][0], image_width, image_height, 1.0f, 2.0f);
  EXPECT_EQ(exp1, res);
  uchar4 exp2 = {240, 160, 90, 20};
  res = interpolate_nearest_border_byte(image_char[0][0], image_width, image_height, 2.0f, 0.0f);
  EXPECT_EQ(exp2, res);
}

TEST(math_interp, NearestCharHalfwaySamples)
{
  uchar4 res;
  uchar4 exp1 = {0, 1, 2, 3};
  res = interpolate_nearest_border_byte(image_char[0][0], image_width, image_height, 0.5f, 1.5f);
  EXPECT_EQ(exp1, res);
  uchar4 exp2 = {255, 254, 217, 216};
  res = interpolate_nearest_border_byte(image_char[0][0], image_width, image_height, 0.5f, 0.5f);
  EXPECT_EQ(exp2, res);
}

TEST(math_interp, NearestFloatExactSamples)
{
  float4 res;
  float4 exp1 = {73.0f, 108.0f, 153.0f, 251.0f};
  res = interpolate_nearest_border_fl(image_fl[0][0], image_width, image_height, 1.0f, 2.0f);
  EXPECT_EQ(exp1, res);
  float4 exp2 = {240.0f, 160.0f, 90.0f, 20.0f};
  res = interpolate_nearest_border_fl(image_fl[0][0], image_width, image_height, 2.0f, 0.0f);
  EXPECT_EQ(exp2, res);
}

TEST(math_interp, NearestFloatHalfwaySamples)
{
  float4 res;
  float4 exp1 = {0.0f, 1.0f, 2.0f, 3.0f};
  res = interpolate_nearest_border_fl(image_fl[0][0], image_width, image_height, 0.5f, 1.5f);
  EXPECT_EQ(exp1, res);
  float4 exp2 = {255.0f, 254.0f, 217.0f, 216.0f};
  res = interpolate_nearest_border_fl(image_fl[0][0], image_width, image_height, 0.5f, 0.5f);
  EXPECT_EQ(exp2, res);
}

TEST(math_interp, BilinearCharExactSamples)
{
  uchar4 res;
  uchar4 exp1 = {73, 108, 153, 251};
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 1.0f, 2.0f);
  EXPECT_EQ(exp1, res);
  uchar4 exp2 = {240, 160, 90, 20};
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 2.0f, 0.0f);
  EXPECT_EQ(exp2, res);
}

TEST(math_interp, BilinearCharHalfwayUSamples)
{
  uchar4 res;
  uchar4 exp1 = {31, 37, 42, 48};
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 0.5f, 1.0f);
  EXPECT_EQ(exp1, res);
  uchar4 exp2 = {243, 242, 224, 223};
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 0.5f, 0.0f);
  EXPECT_EQ(exp2, res);
}

TEST(math_interp, BilinearCharHalfwayVSamples)
{
  uchar4 res;
  uchar4 exp1 = {1, 2, 3, 4};
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 0.0f, 1.5f);
  EXPECT_EQ(exp1, res);
  uchar4 exp2 = {127, 128, 129, 130};
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 2.0f, 1.5f);
  EXPECT_EQ(exp2, res);
}

TEST(math_interp, BilinearCharSamples)
{
  uchar4 res;
  uchar4 exp1 = {136, 133, 132, 130};
  res = interpolate_bilinear_border_byte(
      image_char[0][0], image_width, image_height, 1.25f, 0.625f);
  EXPECT_EQ(exp1, res);
  uchar4 exp2 = {219, 191, 167, 142};
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 1.4f, 0.1f);
  EXPECT_EQ(exp2, res);
}

TEST(math_interp, BilinearFloatSamples)
{
  float4 res;
  float4 exp1 = {135.9375f, 133.28125f, 131.5625f, 129.84375f};
  res = interpolate_bilinear_border_fl(image_fl[0][0], image_width, image_height, 1.25f, 0.625f);
  EXPECT_V4_NEAR(exp1, res, float_tolerance);
  float4 exp2 = {219.36f, 191.2f, 166.64f, 142.08f};
  res = interpolate_bilinear_border_fl(image_fl[0][0], image_width, image_height, 1.4f, 0.1f);
  EXPECT_V4_NEAR(exp2, res, float_tolerance);
}

TEST(math_interp, BilinearCharPartiallyOutsideImageBorder)
{
  uchar4 res;
  uchar4 exp1 = {1, 1, 2, 2};
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, -0.5f, 2.0f);
  EXPECT_EQ(exp1, res);
  uchar4 exp2 = {9, 11, 15, 22};
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 1.25f, 2.9f);
  EXPECT_EQ(exp2, res);
  uchar4 exp3 = {173, 115, 65, 14};
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 2.2f, -0.1f);
  EXPECT_EQ(exp3, res);
}

TEST(math_interp, BilinearCharPartiallyOutsideImage)
{
  uchar4 res;
  uint4 exp1 = {1, 2, 3, 4};
  res = interpolate_bilinear_byte(image_char[0][0], image_width, image_height, -0.5f, 2.0f);
  EXPECT_EQ(exp1, uint4(res));
  uint4 exp2 = {87, 113, 147, 221};
  res = interpolate_bilinear_byte(image_char[0][0], image_width, image_height, 1.25f, 2.9f);
  EXPECT_EQ(exp2, uint4(res));
  uint4 exp3 = {240, 160, 90, 20};
  res = interpolate_bilinear_byte(image_char[0][0], image_width, image_height, 2.2f, -0.1f);
  EXPECT_EQ(exp3, uint4(res));
}

TEST(math_interp, BilinearCharPartiallyOutsideImageWrap)
{
  uchar4 res;
  uchar4 exp1 = {65, 66, 67, 68};
  res = interpolate_bilinear_wrap_byte(image_char[0][0], image_width, image_height, -0.5f, 2.0f);
  EXPECT_EQ(exp1, res);
  uchar4 exp2 = {218, 203, 190, 182};
  res = interpolate_bilinear_wrap_byte(image_char[0][0], image_width, image_height, 1.25f, 2.9f);
  EXPECT_EQ(exp2, res);
  uchar4 exp3 = {229, 171, 114, 64};
  res = interpolate_bilinear_wrap_byte(image_char[0][0], image_width, image_height, 2.2f, -0.1f);
  EXPECT_EQ(exp3, res);
}

TEST(math_interp, BilinearFloatPartiallyOutsideImageBorder)
{
  float4 res;
  float4 exp1 = {0.5f, 1, 1.5f, 2};
  res = interpolate_bilinear_border_fl(image_fl[0][0], image_width, image_height, -0.5f, 2.0f);
  EXPECT_V4_NEAR(exp1, res, float_tolerance);
  float4 exp2 = {8.675f, 11.325f, 14.725f, 22.1f};
  res = interpolate_bilinear_border_fl(image_fl[0][0], image_width, image_height, 1.25f, 2.9f);
  EXPECT_V4_NEAR(exp2, res, float_tolerance);
  float4 exp3 = {172.8f, 115.2f, 64.8f, 14.4f};
  res = interpolate_bilinear_border_fl(image_fl[0][0], image_width, image_height, 2.2f, -0.1f);
  EXPECT_V4_NEAR(exp3, res, float_tolerance);
}

TEST(math_interp, BilinearFloatPartiallyOutsideImage)
{
  float4 res;
  float4 exp1 = {1.0f, 2.0f, 3.0f, 4.0f};
  res = interpolate_bilinear_fl(image_fl[0][0], image_width, image_height, -0.5f, 2.0f);
  EXPECT_V4_NEAR(exp1, res, float_tolerance);
  float4 exp2 = {86.75f, 113.25f, 147.25f, 221.0f};
  res = interpolate_bilinear_fl(image_fl[0][0], image_width, image_height, 1.25f, 2.9f);
  EXPECT_V4_NEAR(exp2, res, float_tolerance);
  float4 exp3 = {240.0f, 160.0f, 90.0f, 20.0f};
  res = interpolate_bilinear_fl(image_fl[0][0], image_width, image_height, 2.2f, -0.1f);
  EXPECT_V4_NEAR(exp3, res, float_tolerance);
}

TEST(math_interp, BilinearFloatPartiallyOutsideImageWrap)
{
  float4 res;
  float4 exp1 = {64.5f, 65.5f, 66.5f, 67.5f};
  interpolate_bilinear_wrap_fl(
      image_fl[0][0], res, image_width, image_height, 4, -0.5f, 2.0f, true, true);
  EXPECT_V4_NEAR(exp1, res, float_tolerance);
  res = interpolate_bilinear_wrap_fl(image_fl[0][0], image_width, image_height, -0.5f, 2.0f);
  EXPECT_V4_NEAR(exp1, res, float_tolerance);

  float4 exp2 = {217.92502f, 202.57501f, 190.22501f, 181.85f};
  interpolate_bilinear_wrap_fl(
      image_fl[0][0], res, image_width, image_height, 4, 1.25f, 2.9f, true, true);
  EXPECT_V4_NEAR(exp2, res, float_tolerance);
  res = interpolate_bilinear_wrap_fl(image_fl[0][0], image_width, image_height, 1.25f, 2.9f);
  EXPECT_V4_NEAR(exp2, res, float_tolerance);

  float4 exp3 = {228.96f, 171.27998f, 114.32f, 63.84f};
  interpolate_bilinear_wrap_fl(
      image_fl[0][0], res, image_width, image_height, 4, 2.2f, -0.1f, true, true);
  EXPECT_V4_NEAR(exp3, res, float_tolerance);
  res = interpolate_bilinear_wrap_fl(image_fl[0][0], image_width, image_height, 2.2f, -0.1f);
  EXPECT_V4_NEAR(exp3, res, float_tolerance);
}

TEST(math_interp, BilinearCharFullyOutsideImage)
{
  uchar4 res;
  uchar4 exp = {0, 0, 0, 0};
  /* Out of range on U */
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, -1.5f, 0);
  EXPECT_EQ(exp, res);
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, -1.1f, 0);
  EXPECT_EQ(exp, res);
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 3, 0);
  EXPECT_EQ(exp, res);
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 5, 0);
  EXPECT_EQ(exp, res);

  /* Out of range on V */
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 0, -3.2f);
  EXPECT_EQ(exp, res);
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 0, -1.5f);
  EXPECT_EQ(exp, res);
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 0, 3.1f);
  EXPECT_EQ(exp, res);
  res = interpolate_bilinear_border_byte(image_char[0][0], image_width, image_height, 0, 500.0f);
  EXPECT_EQ(exp, res);
}

TEST(math_interp, CubicBSplineCharExactSamples)
{
  uchar4 res;
  uchar4 exp1 = {69, 90, 116, 172};
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, 1.0f, 2.0f);
  EXPECT_EQ(exp1, res);
  uchar4 exp2 = {218, 163, 115, 66};
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, 2.0f, 0.0f);
  EXPECT_EQ(exp2, res);
}

TEST(math_interp, CubicBSplineCharSamples)
{
  uchar4 res;
  uchar4 exp1 = {142, 136, 131, 128};
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, 1.25f, 0.625f);
  EXPECT_EQ(exp1, res);
  uchar4 exp2 = {202, 177, 154, 132};
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, 1.4f, 0.1f);
  EXPECT_EQ(exp2, res);
}

TEST(math_interp, CubicBSplineFloatSamples)
{
  float4 res;
  float4 exp1 = {142.14418f, 136.255798f, 130.87924f, 127.85243f};
  res = interpolate_cubic_bspline_fl(image_fl[0][0], image_width, image_height, 1.25f, 0.625f);
  EXPECT_V4_NEAR(exp1, res, float_tolerance);
  float4 exp2 = {202.36082f, 177.13397f, 154.21078f, 132.30153f};
  res = interpolate_cubic_bspline_fl(image_fl[0][0], image_width, image_height, 1.4f, 0.1f);
  EXPECT_V4_NEAR(exp2, res, float_tolerance);
}

TEST(math_interp, CubicBSplineCharPartiallyOutsideImage)
{
  uchar4 res;
  uchar4 exp1 = {2, 4, 6, 8};
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, -0.5f, 2.0f);
  EXPECT_EQ(exp1, res);
  uchar4 exp2 = {85, 107, 135, 195};
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, 1.25f, 2.9f);
  EXPECT_EQ(exp2, res);
  uchar4 exp3 = {225, 161, 105, 49};
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, 2.2f, -0.1f);
  EXPECT_EQ(exp3, res);
}

TEST(math_interp, CubicBSplineFloatPartiallyOutsideImage)
{
  float4 res;
  float4 exp1 = {2.29861f, 3.92014f, 5.71528f, 8.430554f};
  res = interpolate_cubic_bspline_fl(image_fl[0][0], image_width, image_height, -0.5f, 2.0f);
  EXPECT_V4_NEAR(exp1, res, float_tolerance);
  float4 exp2 = {85.41022f, 107.21497f, 135.13849f, 195.49146f};
  res = interpolate_cubic_bspline_fl(image_fl[0][0], image_width, image_height, 1.25f, 2.9f);
  EXPECT_V4_NEAR(exp2, res, float_tolerance);
  float4 exp3 = {224.73579f, 160.66783f, 104.63521f, 48.60260f};
  res = interpolate_cubic_bspline_fl(image_fl[0][0], image_width, image_height, 2.2f, -0.1f);
  EXPECT_V4_NEAR(exp3, res, float_tolerance);
}

TEST(math_interp, CubicBSplineCharFullyOutsideImage)
{
  uchar4 res;
  uchar4 exp = {0, 0, 0, 0};
  /* Out of range on U */
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, -1.5f, 0);
  EXPECT_EQ(exp, res);
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, -1.1f, 0);
  EXPECT_EQ(exp, res);
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, 3, 0);
  EXPECT_EQ(exp, res);
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, 5, 0);
  EXPECT_EQ(exp, res);

  /* Out of range on V */
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, 0, -3.2f);
  EXPECT_EQ(exp, res);
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, 0, -1.5f);
  EXPECT_EQ(exp, res);
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, 0, 3.1f);
  EXPECT_EQ(exp, res);
  res = interpolate_cubic_bspline_byte(image_char[0][0], image_width, image_height, 0, 500.0f);
  EXPECT_EQ(exp, res);
}

TEST(math_interp, CubicMitchellCharExactSamples)
{
  uchar4 res;
  uchar4 exp1 = {72, 101, 140, 223};
  res = interpolate_cubic_mitchell_byte(image_char[0][0], image_width, image_height, 1.0f, 2.0f);
  EXPECT_EQ(int4(exp1), int4(res));
  uchar4 exp2 = {233, 162, 99, 37};
  res = interpolate_cubic_mitchell_byte(image_char[0][0], image_width, image_height, 2.0f, 0.0f);
  EXPECT_EQ(int4(exp2), int4(res));
}

TEST(math_interp, CubicMitchellCharSamples)
{
  uchar4 res;
  uchar4 exp1 = {135, 132, 130, 127};
  res = interpolate_cubic_mitchell_byte(
      image_char[0][0], image_width, image_height, 1.25f, 0.625f);
  EXPECT_EQ(int4(exp1), int4(res));
  uchar4 exp2 = {216, 189, 167, 143};
  res = interpolate_cubic_mitchell_byte(image_char[0][0], image_width, image_height, 1.4f, 0.1f);
  EXPECT_EQ(int4(exp2), int4(res));
}

TEST(math_interp, CubicMitchellFloatSamples)
{
  float4 res;
  float4 exp1 = {134.5659f, 131.91309f, 130.17685f, 126.66989f};
  res = interpolate_cubic_mitchell_fl(image_fl[0][0], image_width, image_height, 1.25f, 0.625f);
  EXPECT_V4_NEAR(exp1, res, float_tolerance);
  float4 exp2 = {216.27115f, 189.30673f, 166.93599f, 143.31964f};
  res = interpolate_cubic_mitchell_fl(image_fl[0][0], image_width, image_height, 1.4f, 0.1f);
  EXPECT_V4_NEAR(exp2, res, float_tolerance);
}

TEST(math_interp, CubicMitchellCharPartiallyOutsideImage)
{
  uchar4 res;
  uchar4 exp1 = {0, 0, 0, 0};
  res = interpolate_cubic_mitchell_byte(image_char[0][0], image_width, image_height, -0.5f, 2.0f);
  EXPECT_EQ(int4(exp1), int4(res));
  uchar4 exp2 = {88, 116, 151, 228};
  res = interpolate_cubic_mitchell_byte(image_char[0][0], image_width, image_height, 1.25f, 2.9f);
  EXPECT_EQ(int4(exp2), int4(res));
  uchar4 exp3 = {239, 159, 89, 19};
  res = interpolate_cubic_mitchell_byte(image_char[0][0], image_width, image_height, 2.2f, -0.1f);
  EXPECT_EQ(int4(exp3), int4(res));
}

TEST(math_interp, CubicMitchellFloatPartiallyOutsideImage)
{
  float4 res;
  float4 exp1 = {0, 0, 0, 0};
  res = interpolate_cubic_mitchell_fl(image_fl[0][0], image_width, image_height, -0.5f, 2.0f);
  EXPECT_V4_NEAR(exp1, res, float_tolerance);
  float4 exp2 = {87.98676f, 115.63634f, 151.13014f, 228.19823f};
  res = interpolate_cubic_mitchell_fl(image_fl[0][0], image_width, image_height, 1.25f, 2.9f);
  EXPECT_V4_NEAR(exp2, res, float_tolerance);
  float4 exp3 = {238.6136f, 158.58293f, 88.55761f, 18.53225f};
  res = interpolate_cubic_mitchell_fl(image_fl[0][0], image_width, image_height, 2.2f, -0.1f);
  EXPECT_V4_NEAR(exp3, res, float_tolerance);
}
