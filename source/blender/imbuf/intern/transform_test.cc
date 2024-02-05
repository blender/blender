/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BLI_color.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_quaternion_types.hh"
#include "IMB_imbuf.hh"

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

static ImBuf *transform_2x_smaller(eIMBInterpolationFilterMode filter)
{
  ImBuf *src = create_6x2_test_image();
  ImBuf *dst = IMB_allocImBuf(3, 1, 32, IB_rect);
  float4x4 matrix = math::from_scale<float4x4>(float4(2.0f));
  IMB_transform(src, dst, IMB_TRANSFORM_MODE_REGULAR, filter, matrix.ptr(), nullptr);
  IMB_freeImBuf(src);
  return dst;
}

static ImBuf *transform_fractional_larger(eIMBInterpolationFilterMode filter)
{
  ImBuf *src = create_6x2_test_image();
  ImBuf *dst = IMB_allocImBuf(9, 7, 32, IB_rect);
  float4x4 matrix = math::from_scale<float4x4>(float4(6.0f / 9.0f, 2.0f / 7.0f, 1.0f, 1.0f));
  IMB_transform(src, dst, IMB_TRANSFORM_MODE_REGULAR, filter, matrix.ptr(), nullptr);
  IMB_freeImBuf(src);
  return dst;
}

TEST(imbuf_transform, nearest_2x_smaller)
{
  ImBuf *res = transform_2x_smaller(IMB_FILTER_NEAREST);
  const ColorTheme4b *got = reinterpret_cast<ColorTheme4b *>(res->byte_buffer.data);
  EXPECT_EQ(got[0], ColorTheme4b(255, 255, 255, 255));
  EXPECT_EQ(got[1], ColorTheme4b(133, 55, 31, 19));
  EXPECT_EQ(got[2], ColorTheme4b(57, 0, 96, 252));
  IMB_freeImBuf(res);
}

TEST(imbuf_transform, box_2x_smaller)
{
  ImBuf *res = transform_2x_smaller(IMB_FILTER_BOX);
  const ColorTheme4b *got = reinterpret_cast<ColorTheme4b *>(res->byte_buffer.data);
  /* At 2x reduction should be same as bilinear, save for some rounding errors. */
  EXPECT_EQ(got[0], ColorTheme4b(191, 128, 64, 255));
  EXPECT_EQ(got[1], ColorTheme4b(133, 55, 31, 16));
  EXPECT_EQ(got[2], ColorTheme4b(54, 50, 48, 254));
  IMB_freeImBuf(res);
}

TEST(imbuf_transform, bilinear_2x_smaller)
{
  ImBuf *res = transform_2x_smaller(IMB_FILTER_BILINEAR);
  const ColorTheme4b *got = reinterpret_cast<ColorTheme4b *>(res->byte_buffer.data);
  EXPECT_EQ(got[0], ColorTheme4b(191, 128, 64, 255));
  EXPECT_EQ(got[1], ColorTheme4b(133, 55, 31, 16));
  EXPECT_EQ(got[2], ColorTheme4b(55, 50, 48, 254));
  IMB_freeImBuf(res);
}

TEST(imbuf_transform, cubic_bspline_2x_smaller)
{
  ImBuf *res = transform_2x_smaller(IMB_FILTER_CUBIC_BSPLINE);
  const ColorTheme4b *got = reinterpret_cast<ColorTheme4b *>(res->byte_buffer.data);
  EXPECT_EQ(got[0], ColorTheme4b(189, 126, 62, 250));
  EXPECT_EQ(got[1], ColorTheme4b(134, 57, 33, 26));
  EXPECT_EQ(got[2], ColorTheme4b(56, 49, 48, 249));
  IMB_freeImBuf(res);
}

TEST(imbuf_transform, cubic_mitchell_2x_smaller)
{
  ImBuf *res = transform_2x_smaller(IMB_FILTER_CUBIC_MITCHELL);
  const ColorTheme4b *got = reinterpret_cast<ColorTheme4b *>(res->byte_buffer.data);
  EXPECT_EQ(got[0], ColorTheme4b(195, 130, 67, 255));
  EXPECT_EQ(got[1], ColorTheme4b(132, 51, 28, 0));
  EXPECT_EQ(got[2], ColorTheme4b(52, 52, 48, 255));
  IMB_freeImBuf(res);
}

TEST(imbuf_transform, cubic_mitchell_fractional_larger)
{
  ImBuf *res = transform_fractional_larger(IMB_FILTER_CUBIC_MITCHELL);
  const ColorTheme4b *got = reinterpret_cast<ColorTheme4b *>(res->byte_buffer.data);
  EXPECT_EQ(got[0 + 0 * res->x], ColorTheme4b(0, 0, 0, 255));
  EXPECT_EQ(got[1 + 0 * res->x], ColorTheme4b(127, 0, 0, 255));
  EXPECT_EQ(got[7 + 0 * res->x], ColorTheme4b(49, 109, 13, 255));
  EXPECT_EQ(got[2 + 2 * res->x], ColorTheme4b(236, 53, 50, 215));
  EXPECT_EQ(got[3 + 2 * res->x], ColorTheme4b(155, 55, 35, 54));
  EXPECT_EQ(got[8 + 6 * res->x], ColorTheme4b(57, 0, 98, 252));
  IMB_freeImBuf(res);
}

TEST(imbuf_transform, nearest_very_large_scale)
{
  /* Create 511x1 black image, with three middle pixels being red/green/blue. */
  ImBuf *src = IMB_allocImBuf(511, 1, 32, IB_rect);
  ColorTheme4b col_r = ColorTheme4b(255, 0, 0, 255);
  ColorTheme4b col_g = ColorTheme4b(0, 255, 0, 255);
  ColorTheme4b col_b = ColorTheme4b(0, 0, 255, 255);
  ColorTheme4b col_0 = ColorTheme4b(0, 0, 0, 0);
  ColorTheme4b *src_col = reinterpret_cast<ColorTheme4b *>(src->byte_buffer.data);
  src_col[254] = col_r;
  src_col[255] = col_g;
  src_col[256] = col_b;

  /* Create 3841x1 image, and scale the input image so that the three middle
   * pixels cover almost all of it, except the rightmost pixel. */
  ImBuf *res = IMB_allocImBuf(3841, 1, 32, IB_rect);
  float4x4 matrix = math::from_loc_rot_scale<float4x4>(
      float3(254, 0, 0), math::Quaternion::identity(), float3(3.0f / 3840.0f, 1, 1));
  IMB_transform(src, res, IMB_TRANSFORM_MODE_REGULAR, IMB_FILTER_NEAREST, matrix.ptr(), nullptr);

  /* Check result: leftmost red, middle green, two rightmost pixels blue and black.
   * If the transform code internally does not have enough precision while stepping
   * through the scan-line, the rightmost side will not come out correctly. */
  const ColorTheme4b *got = reinterpret_cast<ColorTheme4b *>(res->byte_buffer.data);
  EXPECT_EQ(got[0], col_r);
  EXPECT_EQ(got[res->x / 2], col_g);
  EXPECT_EQ(got[res->x - 2], col_b);
  EXPECT_EQ(got[res->x - 1], col_0);
  IMB_freeImBuf(src);
  IMB_freeImBuf(res);
}

}  // namespace blender::imbuf::tests
