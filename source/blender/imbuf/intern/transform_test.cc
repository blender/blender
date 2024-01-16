/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BLI_color.hh"
#include "BLI_math_matrix.hh"
#include "IMB_imbuf.h"

namespace blender::imbuf::tests {

static ImBuf *create_6x2_test_image()
{
  ImBuf *img = IMB_allocImBuf(6, 2, 32, IB_rect);
  ColorTheme4b *col = reinterpret_cast<ColorTheme4b *>(img->byte_buffer.data);

  /* Source pixels are spelled out in 2x2 blocks below:
   * nearest filter results in corner pixel from each block, bilinear
   * is average of each block. */
  col[0] = ColorTheme4b(0, 0, 0, 255);
  col[1] = ColorTheme4b(255, 0, 0, 255);
  col[6] = ColorTheme4b(255, 255, 0, 255);
  col[7] = ColorTheme4b(255, 255, 255, 255);

  col[2] = ColorTheme4b(133, 55, 31, 13);
  col[3] = ColorTheme4b(133, 55, 31, 15);
  col[8] = ColorTheme4b(133, 55, 31, 17);
  col[9] = ColorTheme4b(133, 55, 31, 19);

  col[4] = ColorTheme4b(50, 200, 0, 255);
  col[5] = ColorTheme4b(55, 0, 32, 254);
  col[10] = ColorTheme4b(56, 0, 64, 253);
  col[11] = ColorTheme4b(57, 0, 96, 252);

  return img;
}

static ImBuf *transform_2x_smaller(eIMBInterpolationFilterMode filter, int subsamples)
{
  ImBuf *src = create_6x2_test_image();
  ImBuf *dst = IMB_allocImBuf(3, 1, 32, IB_rect);
  float4x4 matrix = math::from_scale<float4x4>(float4(2.0f));
  IMB_transform(src, dst, IMB_TRANSFORM_MODE_REGULAR, filter, subsamples, matrix.ptr(), nullptr);
  IMB_freeImBuf(src);
  return dst;
}

static ImBuf *transform_fractional_larger(eIMBInterpolationFilterMode filter, int subsamples)
{
  ImBuf *src = create_6x2_test_image();
  ImBuf *dst = IMB_allocImBuf(9, 7, 32, IB_rect);
  float4x4 matrix = math::from_scale<float4x4>(float4(6.0f / 9.0f, 2.0f / 7.0f, 1.0f, 1.0f));
  IMB_transform(src, dst, IMB_TRANSFORM_MODE_REGULAR, filter, subsamples, matrix.ptr(), nullptr);
  IMB_freeImBuf(src);
  return dst;
}

TEST(imbuf_transform, nearest_2x_smaller)
{
  ImBuf *res = transform_2x_smaller(IMB_FILTER_NEAREST, 1);
  const ColorTheme4b *got = reinterpret_cast<ColorTheme4b *>(res->byte_buffer.data);
  EXPECT_EQ(got[0], ColorTheme4b(255, 255, 255, 255));
  EXPECT_EQ(got[1], ColorTheme4b(133, 55, 31, 19));
  EXPECT_EQ(got[2], ColorTheme4b(57, 0, 96, 252));
  IMB_freeImBuf(res);
}

TEST(imbuf_transform, nearest_subsample3_2x_smaller)
{
  ImBuf *res = transform_2x_smaller(IMB_FILTER_NEAREST, 3);
  const ColorTheme4b *got = reinterpret_cast<ColorTheme4b *>(res->byte_buffer.data);
  EXPECT_EQ(got[0], ColorTheme4b(226, 168, 113, 255));
  EXPECT_EQ(got[1], ColorTheme4b(133, 55, 31, 16));
  EXPECT_EQ(got[2], ColorTheme4b(55, 22, 64, 254));
  IMB_freeImBuf(res);
}

TEST(imbuf_transform, bilinear_2x_smaller)
{
  ImBuf *res = transform_2x_smaller(IMB_FILTER_BILINEAR, 1);
  const ColorTheme4b *got = reinterpret_cast<ColorTheme4b *>(res->byte_buffer.data);
  EXPECT_EQ(got[0], ColorTheme4b(191, 128, 64, 255));
  EXPECT_EQ(got[1], ColorTheme4b(133, 55, 31, 16));
  EXPECT_EQ(got[2], ColorTheme4b(55, 50, 48, 254));
  IMB_freeImBuf(res);
}

TEST(imbuf_transform, bicubic_2x_smaller)
{
  ImBuf *res = transform_2x_smaller(IMB_FILTER_BICUBIC, 1);
  const ColorTheme4b *got = reinterpret_cast<ColorTheme4b *>(res->byte_buffer.data);
  EXPECT_EQ(got[0], ColorTheme4b(189, 126, 62, 250));
  EXPECT_EQ(got[1], ColorTheme4b(134, 57, 33, 26));
  EXPECT_EQ(got[2], ColorTheme4b(56, 49, 48, 249));
  IMB_freeImBuf(res);
}

TEST(imbuf_transform, bicubic_fractional_larger)
{
  ImBuf *res = transform_fractional_larger(IMB_FILTER_BICUBIC, 1);
  const ColorTheme4b *got = reinterpret_cast<ColorTheme4b *>(res->byte_buffer.data);
  EXPECT_EQ(got[0 + 0 * res->x], ColorTheme4b(35, 11, 1, 255));
  EXPECT_EQ(got[1 + 0 * res->x], ColorTheme4b(131, 12, 6, 250));
  EXPECT_EQ(got[7 + 0 * res->x], ColorTheme4b(54, 93, 19, 249));
  EXPECT_EQ(got[2 + 2 * res->x], ColorTheme4b(206, 70, 56, 192));
  EXPECT_EQ(got[3 + 2 * res->x], ColorTheme4b(165, 60, 42, 78));
  EXPECT_EQ(got[8 + 6 * res->x], ColorTheme4b(57, 1, 90, 252));
  IMB_freeImBuf(res);
}

}  // namespace blender::imbuf::tests
