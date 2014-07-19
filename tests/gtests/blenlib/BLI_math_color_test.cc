/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_math.h"

TEST(math_color, RGBToHSVRoundtrip)
{
	float orig_rgb[3] = {0.1f, 0.2f, 0.3f};
	float hsv[3], rgb[3];
	rgb_to_hsv_v(orig_rgb, hsv);
	hsv_to_rgb_v(hsv, rgb);
	EXPECT_V3_NEAR(orig_rgb, rgb, 1e-5);
}

TEST(math_color, RGBToHSLRoundtrip)
{
	float orig_rgb[3] = {0.1f, 0.2f, 0.3f};
	float hsl[3], rgb[3];
	rgb_to_hsl_v(orig_rgb, hsl);
	hsl_to_rgb_v(hsl, rgb);
	EXPECT_V3_NEAR(orig_rgb, rgb, 1e-5);
}

TEST(math_color, RGBToYUVRoundtrip)
{
	float orig_rgb[3] = {0.1f, 0.2f, 0.3f};
	float yuv[3], rgb[3];
	rgb_to_yuv(orig_rgb[0], orig_rgb[1], orig_rgb[2],
	           &yuv[0], &yuv[1], &yuv[2]);
	yuv_to_rgb(yuv[0], yuv[1], yuv[2],
	           &rgb[0], &rgb[1], &rgb[2]);
	EXPECT_V3_NEAR(orig_rgb, rgb, 1e-4);
}

TEST(math_color, RGBToYCCRoundtrip)
{
	float orig_rgb[3] = {0.1f, 0.2f, 0.3f};
	float ycc[3], rgb[3];

	rgb_to_ycc(orig_rgb[0], orig_rgb[1], orig_rgb[2],
	           &ycc[0], &ycc[1], &ycc[2],
	           BLI_YCC_ITU_BT601);
	ycc_to_rgb(ycc[0], ycc[1], ycc[2],
	           &rgb[0], &rgb[1], &rgb[2],
	           BLI_YCC_ITU_BT601);
	EXPECT_V3_NEAR(orig_rgb, rgb, 1e-3);

	rgb_to_ycc(orig_rgb[0], orig_rgb[1], orig_rgb[2],
	           &ycc[0], &ycc[1], &ycc[2],
	           BLI_YCC_ITU_BT709);
	ycc_to_rgb(ycc[0], ycc[1], ycc[2],
	           &rgb[0], &rgb[1], &rgb[2],
	           BLI_YCC_ITU_BT709);
	EXPECT_V3_NEAR(orig_rgb, rgb, 1e-3);

	rgb_to_ycc(orig_rgb[0], orig_rgb[1], orig_rgb[2],
	           &ycc[0], &ycc[1], &ycc[2],
	           BLI_YCC_JFIF_0_255);
	ycc_to_rgb(ycc[0], ycc[1], ycc[2],
	           &rgb[0], &rgb[1], &rgb[2],
	           BLI_YCC_JFIF_0_255);
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
		float orig_linear_color = (float) i / N;
		float srgb_color = linearrgb_to_srgb(orig_linear_color);
		float linear_color = srgb_to_linearrgb(srgb_color);
		EXPECT_NEAR(orig_linear_color, linear_color, 1e-5);
	}
}
