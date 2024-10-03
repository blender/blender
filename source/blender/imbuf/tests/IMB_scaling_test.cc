/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BLI_math_vector_types.hh"
#include "IMB_imbuf.hh"

namespace blender::imbuf::tests {

static ImBuf *create_6x2_test_image()
{
  ImBuf *img = IMB_allocImBuf(6, 2, 32, IB_rect);
  uchar4 *col = reinterpret_cast<uchar4 *>(img->byte_buffer.data);

  /* Source pixels are spelled out in 2x2 blocks below:
   * nearest filter results in corner pixel from each block, bilinear
   * is average of each block. */
  col[0] = uchar4(0, 0, 0, 255);
  col[1] = uchar4(255, 0, 0, 255);
  col[6] = uchar4(255, 255, 0, 255);
  col[7] = uchar4(255, 255, 255, 255);

  col[2] = uchar4(133, 55, 31, 13);
  col[3] = uchar4(133, 55, 31, 15);
  col[8] = uchar4(133, 55, 31, 17);
  col[9] = uchar4(133, 55, 31, 19);

  col[4] = uchar4(50, 200, 0, 255);
  col[5] = uchar4(55, 0, 32, 254);
  col[10] = uchar4(56, 0, 64, 253);
  col[11] = uchar4(57, 0, 96, 252);

  return img;
}

static ImBuf *create_6x2_test_image_fl(int channels)
{
  ImBuf *img = IMB_allocImBuf(6, 2, 32, IB_rectfloat);
  img->channels = channels;
  float *col = img->float_buffer.data;

  for (int y = 0; y < img->y; y++) {
    for (int x = 0; x < img->x; x++) {
      for (int ch = 0; ch < channels; ch++) {
        *col = x * 1.25f + y * 0.5f + ch * 0.125f;
        col++;
      }
    }
  }
  return img;
}

static ImBuf *scale_2x_smaller(bool nearest, bool threaded, int float_channels = 0)
{
  ImBuf *img = float_channels > 0 ? create_6x2_test_image_fl(float_channels) :
                                    create_6x2_test_image();
  int ww = 3, hh = 1;
  if (threaded) {
    IMB_scale(img, ww, hh, IMBScaleFilter::Bilinear, true);
  }
  else if (nearest) {
    IMB_scale(img, ww, hh, IMBScaleFilter::Nearest, false);
  }
  else {
    IMB_scale(img, ww, hh, IMBScaleFilter::Box, false);
  }
  return img;
}

static ImBuf *scale_to_1x1(bool nearest, bool threaded, int float_channels = 0)
{
  ImBuf *img = float_channels > 0 ? create_6x2_test_image_fl(float_channels) :
                                    create_6x2_test_image();
  int ww = 1, hh = 1;
  if (threaded) {
    IMB_scale(img, ww, hh, IMBScaleFilter::Bilinear, true);
  }
  else if (nearest) {
    IMB_scale(img, ww, hh, IMBScaleFilter::Nearest, false);
  }
  else {
    IMB_scale(img, ww, hh, IMBScaleFilter::Box, false);
  }
  return img;
}

static ImBuf *scale_fractional_larger(bool nearest, bool threaded, int float_channels = 0)
{
  ImBuf *img = float_channels > 0 ? create_6x2_test_image_fl(float_channels) :
                                    create_6x2_test_image();
  int ww = 9, hh = 7;
  if (threaded) {
    IMB_scale(img, ww, hh, IMBScaleFilter::Bilinear, true);
  }
  else if (nearest) {
    IMB_scale(img, ww, hh, IMBScaleFilter::Nearest, false);
  }
  else {
    IMB_scale(img, ww, hh, IMBScaleFilter::Box, false);
  }
  return img;
}

TEST(imbuf_scaling, nearest_2x_smaller)
{
  ImBuf *res = scale_2x_smaller(true, false);
  const uchar4 *got = reinterpret_cast<uchar4 *>(res->byte_buffer.data);
  EXPECT_EQ(uint4(got[0]), uint4(0, 0, 0, 255));
  EXPECT_EQ(uint4(got[1]), uint4(133, 55, 31, 13));
  EXPECT_EQ(uint4(got[2]), uint4(50, 200, 0, 255));
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, threaded_2x_smaller)
{
  ImBuf *res = scale_2x_smaller(false, true);
  const uchar4 *got = reinterpret_cast<uchar4 *>(res->byte_buffer.data);
  EXPECT_EQ(uint4(got[0]), uint4(191, 128, 64, 255));
  EXPECT_EQ(uint4(got[1]), uint4(133, 55, 31, 16));
  EXPECT_EQ(uint4(got[2]), uint4(55, 50, 48, 254));
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, bilinear_2x_smaller)
{
  ImBuf *res = scale_2x_smaller(false, false);
  const uchar4 *got = reinterpret_cast<uchar4 *>(res->byte_buffer.data);
  /* NOTE: #IMB_transform results in (191, 128, 64, 255), <same>,
   * (55, 50, 48, 254) i.e. different rounding. */
  EXPECT_EQ(uint4(got[0]), uint4(191, 127, 63, 255));
  EXPECT_EQ(uint4(got[1]), uint4(133, 55, 31, 16));
  EXPECT_EQ(uint4(got[2]), uint4(55, 50, 48, 253));
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, nearest_to_1x1)
{
  ImBuf *res = scale_to_1x1(true, false);
  const uchar4 *got = reinterpret_cast<uchar4 *>(res->byte_buffer.data);
  EXPECT_EQ(uint4(got[0]), uint4(0, 0, 0, 255));
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, threaded_to_1x1)
{
  ImBuf *res = scale_to_1x1(false, true);
  const uchar4 *got = reinterpret_cast<uchar4 *>(res->byte_buffer.data);
  EXPECT_EQ(uint4(got[0]), uint4(133, 55, 31, 16));
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, bilinear_to_1x1)
{
  ImBuf *res = scale_to_1x1(false, false);
  const uchar4 *got = reinterpret_cast<uchar4 *>(res->byte_buffer.data);
  EXPECT_EQ(uint4(got[0]), uint4(126, 78, 47, 174));
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, nearest_fractional_larger)
{
  ImBuf *res = scale_fractional_larger(true, false);
  const uchar4 *got = reinterpret_cast<uchar4 *>(res->byte_buffer.data);
  EXPECT_EQ(uint4(got[0 + 0 * res->x]), uint4(0, 0, 0, 255));
  EXPECT_EQ(uint4(got[1 + 0 * res->x]), uint4(0, 0, 0, 255));
  EXPECT_EQ(uint4(got[7 + 0 * res->x]), uint4(50, 200, 0, 255));
  EXPECT_EQ(uint4(got[2 + 2 * res->x]), uint4(255, 0, 0, 255));
  EXPECT_EQ(uint4(got[3 + 2 * res->x]), uint4(133, 55, 31, 13));
  EXPECT_EQ(uint4(got[8 + 6 * res->x]), uint4(57, 0, 96, 252));
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, bilinear_fractional_larger)
{
  ImBuf *res = scale_fractional_larger(false, false);
  const uchar4 *got = reinterpret_cast<uchar4 *>(res->byte_buffer.data);
  EXPECT_EQ(uint4(got[0 + 0 * res->x]), uint4(0, 0, 0, 255));
  EXPECT_EQ(uint4(got[1 + 0 * res->x]), uint4(127, 0, 0, 255));
  EXPECT_EQ(uint4(got[7 + 0 * res->x]), uint4(52, 100, 16, 255));
  EXPECT_EQ(uint4(got[2 + 2 * res->x]), uint4(235, 55, 51, 215));
  EXPECT_EQ(uint4(got[3 + 2 * res->x]), uint4(153, 55, 35, 54));
  EXPECT_EQ(uint4(got[8 + 5 * res->x]), uint4(57, 0, 91, 252));
  EXPECT_EQ(uint4(got[0 + 6 * res->x]), uint4(164, 164, 0, 255));
  EXPECT_EQ(uint4(got[7 + 6 * res->x]), uint4(55, 36, 57, 254));
  EXPECT_EQ(uint4(got[8 + 6 * res->x]), uint4(56, 0, 73, 253));
  IMB_freeImBuf(res);
}

static constexpr float EPS = 0.0001f;

TEST(imbuf_scaling, nearest_2x_smaller_fl1)
{
  ImBuf *res = scale_2x_smaller(true, false, 1);
  const float *got = res->float_buffer.data;
  EXPECT_NEAR(got[0], 0.0f, EPS);
  EXPECT_NEAR(got[1], 2.5f, EPS);
  EXPECT_NEAR(got[2], 5.0f, EPS);
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, nearest_2x_smaller_fl2)
{
  ImBuf *res = scale_2x_smaller(true, false, 2);
  const float2 *got = reinterpret_cast<float2 *>(res->float_buffer.data);
  EXPECT_V2_NEAR(got[0], float2(0.0f, 0.125f), EPS);
  EXPECT_V2_NEAR(got[1], float2(2.5f, 2.625f), EPS);
  EXPECT_V2_NEAR(got[2], float2(5.0f, 5.125f), EPS);
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, nearest_2x_smaller_fl3)
{
  ImBuf *res = scale_2x_smaller(true, false, 3);
  const float3 *got = reinterpret_cast<float3 *>(res->float_buffer.data);
  EXPECT_V3_NEAR(got[0], float3(0.0f, 0.125f, 0.25f), EPS);
  EXPECT_V3_NEAR(got[1], float3(2.5f, 2.625f, 2.75f), EPS);
  EXPECT_V3_NEAR(got[2], float3(5.0f, 5.125f, 5.25f), EPS);
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, nearest_2x_smaller_fl4)
{
  ImBuf *res = scale_2x_smaller(true, false, 4);
  const float4 *got = reinterpret_cast<float4 *>(res->float_buffer.data);
  EXPECT_V4_NEAR(got[0], float4(0.0f, 0.125f, 0.25f, 0.375f), EPS);
  EXPECT_V4_NEAR(got[1], float4(2.5f, 2.625f, 2.75f, 2.875f), EPS);
  EXPECT_V4_NEAR(got[2], float4(5.0f, 5.125f, 5.25f, 5.375f), EPS);
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, nearest_to_1x1_fl3)
{
  ImBuf *res = scale_to_1x1(true, false, 3);
  const float3 *got = reinterpret_cast<float3 *>(res->float_buffer.data);
  EXPECT_V3_NEAR(got[0], float3(0, 0.125f, 0.25f), EPS);
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, threaded_to_1x1_fl3)
{
  ImBuf *res = scale_to_1x1(false, true, 3);
  const float3 *got = reinterpret_cast<float3 *>(res->float_buffer.data);
  EXPECT_V3_NEAR(got[0], float3(3.375f, 3.5f, 3.625f), EPS);
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, bilinear_to_1x1_fl3)
{
  ImBuf *res = scale_to_1x1(false, false, 3);
  const float3 *got = reinterpret_cast<float3 *>(res->float_buffer.data);
  EXPECT_V3_NEAR(got[0], float3(3.36853f, 3.49353f, 3.61853f), EPS);
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, bilinear_2x_smaller_fl3)
{
  ImBuf *res = scale_2x_smaller(false, false, 3);
  const float3 *got = reinterpret_cast<float3 *>(res->float_buffer.data);
  EXPECT_V3_NEAR(got[0], float3(0.87270f, 0.99770f, 1.12270f), EPS);
  EXPECT_V3_NEAR(got[1], float3(3.36853f, 3.49353f, 3.61853f), EPS);
  EXPECT_V3_NEAR(got[2], float3(5.86435f, 5.98935f, 6.11435f), EPS);
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, bilinear_2x_smaller_fl4)
{
  ImBuf *res = scale_2x_smaller(false, false, 4);
  const float4 *got = reinterpret_cast<float4 *>(res->float_buffer.data);
  EXPECT_V4_NEAR(got[0], float4(0.87270f, 0.99770f, 1.12270f, 1.24770f), EPS);
  EXPECT_V4_NEAR(got[1], float4(3.36853f, 3.49353f, 3.61853f, 3.74353f), EPS);
  EXPECT_V4_NEAR(got[2], float4(5.86435f, 5.98935f, 6.11435f, 6.23935f), EPS);
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, threaded_2x_smaller_fl3)
{
  ImBuf *res = scale_2x_smaller(false, true, 3);
  const float3 *got = reinterpret_cast<float3 *>(res->float_buffer.data);
  EXPECT_V3_NEAR(got[0], float3(0.875f, 1.0f, 1.125f), EPS);
  EXPECT_V3_NEAR(got[1], float3(3.375f, 3.5f, 3.625f), EPS);
  EXPECT_V3_NEAR(got[2], float3(5.875f, 6.0f, 6.125f), EPS);
  IMB_freeImBuf(res);
}

TEST(imbuf_scaling, threaded_2x_smaller_fl4)
{
  ImBuf *res = scale_2x_smaller(false, true, 4);
  const float4 *got = reinterpret_cast<float4 *>(res->float_buffer.data);
  EXPECT_V4_NEAR(got[0], float4(0.875f, 1.0f, 1.125f, 1.25f), EPS);
  EXPECT_V4_NEAR(got[1], float4(3.375f, 3.5f, 3.625f, 3.75f), EPS);
  EXPECT_V4_NEAR(got[2], float4(5.875f, 6.0f, 6.125f, 6.25f), EPS);
  IMB_freeImBuf(res);
}

}  // namespace blender::imbuf::tests
