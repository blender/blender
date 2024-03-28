/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_color.h"

TEST(math_color, RGBToHSVRoundtrip)
{
  const float orig_rgb[3] = {0.1f, 0.2f, 0.3f};
  float hsv[3], rgb[3];
  rgb_to_hsv_v(orig_rgb, hsv);
  hsv_to_rgb_v(hsv, rgb);
  EXPECT_V3_NEAR(orig_rgb, rgb, 1e-5);
}

TEST(math_color, RGBToHSLRoundtrip)
{
  const float orig_rgb[3] = {0.1f, 0.2f, 0.3f};
  float hsl[3], rgb[3];
  rgb_to_hsl_v(orig_rgb, hsl);
  hsl_to_rgb_v(hsl, rgb);
  EXPECT_V3_NEAR(orig_rgb, rgb, 1e-5);
}

TEST(math_color, RGBToYUVRoundtrip)
{
  const float orig_rgb[3] = {0.1f, 0.2f, 0.3f};
  float yuv[3], rgb[3];
  rgb_to_yuv(orig_rgb[0], orig_rgb[1], orig_rgb[2], &yuv[0], &yuv[1], &yuv[2], BLI_YUV_ITU_BT709);
  yuv_to_rgb(yuv[0], yuv[1], yuv[2], &rgb[0], &rgb[1], &rgb[2], BLI_YUV_ITU_BT709);
  EXPECT_V3_NEAR(orig_rgb, rgb, 1e-4);
}

TEST(math_color, RGBToYCCRoundtrip)
{
  const float orig_rgb[3] = {0.1f, 0.2f, 0.3f};
  float ycc[3], rgb[3];

  rgb_to_ycc(orig_rgb[0], orig_rgb[1], orig_rgb[2], &ycc[0], &ycc[1], &ycc[2], BLI_YCC_ITU_BT601);
  ycc_to_rgb(ycc[0], ycc[1], ycc[2], &rgb[0], &rgb[1], &rgb[2], BLI_YCC_ITU_BT601);
  EXPECT_V3_NEAR(orig_rgb, rgb, 1e-3);

  rgb_to_ycc(orig_rgb[0], orig_rgb[1], orig_rgb[2], &ycc[0], &ycc[1], &ycc[2], BLI_YCC_ITU_BT709);
  ycc_to_rgb(ycc[0], ycc[1], ycc[2], &rgb[0], &rgb[1], &rgb[2], BLI_YCC_ITU_BT709);
  EXPECT_V3_NEAR(orig_rgb, rgb, 1e-3);

  rgb_to_ycc(orig_rgb[0], orig_rgb[1], orig_rgb[2], &ycc[0], &ycc[1], &ycc[2], BLI_YCC_JFIF_0_255);
  ycc_to_rgb(ycc[0], ycc[1], ycc[2], &rgb[0], &rgb[1], &rgb[2], BLI_YCC_JFIF_0_255);
  EXPECT_V3_NEAR(orig_rgb, rgb, 1e-3);
}

TEST(math_color, LinearRGBTosRGBNearZero)
{
  float linear_color = 0.002f;
  float srgb_color = linearrgb_to_srgb(linear_color);
  EXPECT_NEAR(0.02584f, srgb_color, 1e-5);
}

TEST(math_color, LinearRGBTosRGB)
{
  float linear_color = 0.75f;
  float srgb_color = linearrgb_to_srgb(linear_color);
  EXPECT_NEAR(0.880824f, srgb_color, 1e-5);
}

TEST(math_color, LinearRGBTosRGBRoundtrip)
{
  const int N = 50;
  int i;
  for (i = 0; i < N; ++i) {
    float orig_linear_color = float(i) / N;
    float srgb_color = linearrgb_to_srgb(orig_linear_color);
    float linear_color = srgb_to_linearrgb(srgb_color);
    EXPECT_NEAR(orig_linear_color, linear_color, 1e-5);
  }
}

TEST(math_color, linearrgb_to_srgb_v3_v3)
{
  float srgb_color[3];
  {
    const float kTolerance = 1.0e-8f;
    const float linear_color[3] = {0.0023f, 0.0024f, 0.0025f};
    linearrgb_to_srgb_v3_v3(srgb_color, linear_color);
    EXPECT_NEAR(0.029716f, srgb_color[0], kTolerance);
    EXPECT_NEAR(0.031008f, srgb_color[1], kTolerance);
    EXPECT_NEAR(0.032300f, srgb_color[2], kTolerance);
  }

  {
    /* SIMD implementation of linear->srgb for larger inputs
     * is less accurate; use larger tolerance. */
    const float kTolerance = 3.6e-5f;
    const float linear_color[3] = {0.71f, 0.75f, 0.78f};
    linearrgb_to_srgb_v3_v3(srgb_color, linear_color);
    EXPECT_NEAR(0.859696f, srgb_color[0], kTolerance);
    EXPECT_NEAR(0.880825f, srgb_color[1], kTolerance);
    EXPECT_NEAR(0.896244f, srgb_color[2], kTolerance);
  }

  {
    /* Not a common, but possible case: values beyond 1.0 range. */
    const float kTolerance = 2.3e-4f;
    const float linear_color[3] = {1.5f, 2.8f, 5.6f};
    linearrgb_to_srgb_v3_v3(srgb_color, linear_color);
    EXPECT_NEAR(1.19418f, srgb_color[0], kTolerance);
    EXPECT_NEAR(1.56520f, srgb_color[1], kTolerance);
    EXPECT_NEAR(2.10771f, srgb_color[2], kTolerance);
  }
}

TEST(math_color, srgb_to_linearrgb_v3_v3)
{
  float linear_color[3];
  {
    const float kTolerance = 1.0e-8f;
    const float srgb_color[3] = {0.0023f, 0.0024f, 0.0025f};
    srgb_to_linearrgb_v3_v3(linear_color, srgb_color);
    EXPECT_NEAR(0.000178019f, linear_color[0], kTolerance);
    EXPECT_NEAR(0.000185759f, linear_color[1], kTolerance);
    EXPECT_NEAR(0.000193498f, linear_color[2], kTolerance);
  }

  {
    /* SIMD implementation of linear->srgb for larger inputs
     * is less accurate; use larger tolerance. */
    const float kTolerance = 1.5e-7f;
    const float srgb_color[3] = {0.71f, 0.72f, 0.73f};
    srgb_to_linearrgb_v3_v3(linear_color, srgb_color);
    EXPECT_NEAR(0.4623615f, linear_color[0], kTolerance);
    EXPECT_NEAR(0.4770000f, linear_color[1], kTolerance);
    EXPECT_NEAR(0.4919052f, linear_color[2], kTolerance);
  }

  {
    /* Not a common, but possible case: values beyond 1.0 range. */
    const float kTolerance = 7.7e-6f;
    const float srgb_color[3] = {1.1f, 2.5f, 5.6f};
    srgb_to_linearrgb_v3_v3(linear_color, srgb_color);
    EXPECT_NEAR(1.24277f, linear_color[0], kTolerance);
    EXPECT_NEAR(8.35473f, linear_color[1], kTolerance);
    EXPECT_NEAR(56.23833f, linear_color[2], kTolerance);
  }
}
