/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "IMB_imbuf.hh"

#include "BLI_math_base.hh"
#include "BLI_math_matrix.hh"
#include "BLI_timeit.hh"

using namespace blender;

static constexpr int SRC_X = 5123;
static constexpr int SRC_Y = 4091;

static constexpr int DST_SMALLER_X = int(SRC_X * 0.21f);
static constexpr int DST_SMALLER_Y = int(SRC_Y * 0.67f);

static constexpr int DST_LARGER_X = int(SRC_X * 1.19f);
static constexpr int DST_LARGER_Y = int(SRC_Y * 2.13f);

static ImBuf *create_src_image(bool use_float)
{
  ImBuf *img = IMB_allocImBuf(SRC_X, SRC_Y, 32, use_float ? IB_rectfloat : IB_rect);
  if (use_float) {
    float *pix = img->float_buffer.data;
    for (int i = 0; i < img->x * img->y; i++) {
      pix[0] = i * 0.1f;
      pix[1] = i * 2.1f;
      pix[2] = i * 0.01f;
      pix[3] = math::mod(i * 0.03f, 2.0f);
      pix += 4;
    }
  }
  else {
    uchar *pix = img->byte_buffer.data;
    for (int i = 0; i < img->x * img->y; i++) {
      pix[0] = i & 0xFF;
      pix[1] = (i * 3) & 0xFF;
      pix[2] = (i + 12345) & 0xFF;
      pix[3] = (i / 4) & 0xFF;
      pix += 4;
    }
  }
  return img;
}

static void imb_scale_via_transform(ImBuf *&src,
                                    int width,
                                    int height,
                                    eIMBInterpolationFilterMode filter)
{
  ImBuf *dst = IMB_allocImBuf(width, height, src->planes, src->flags);
  float4x4 matrix = math::from_scale<float4x4>(
      float4(float(src->x) / dst->x, float(src->y) / dst->y, 1.0f, 1.0f));
  IMB_transform(src, dst, IMB_TRANSFORM_MODE_REGULAR, filter, matrix.ptr(), nullptr);
  IMB_freeImBuf(src);
  src = dst;
}

static void imb_xform_nearest(ImBuf *&src, int width, int height)
{
  imb_scale_via_transform(src, width, height, IMB_FILTER_NEAREST);
}
static void imb_xform_bilinear(ImBuf *&src, int width, int height)
{
  imb_scale_via_transform(src, width, height, IMB_FILTER_BILINEAR);
}
static void imb_xform_box(ImBuf *&src, int width, int height)
{
  imb_scale_via_transform(src,
                          width,
                          height,
                          width < src->x && height < src->y ? IMB_FILTER_BOX :
                                                              IMB_FILTER_BILINEAR);
}
static void imb_scale_nearest_st(ImBuf *&src, int width, int height)
{
  IMB_scale(src, width, height, IMBScaleFilter::Nearest, false);
}
static void imb_scale_nearest(ImBuf *&src, int width, int height)
{
  IMB_scale(src, width, height, IMBScaleFilter::Nearest, true);
}
static void imb_scale_bilinear_st(ImBuf *&src, int width, int height)
{
  IMB_scale(src, width, height, IMBScaleFilter::Bilinear, false);
}
static void imb_scale_bilinear(ImBuf *&src, int width, int height)
{
  IMB_scale(src, width, height, IMBScaleFilter::Bilinear, true);
}
static void imb_scale_box_st(ImBuf *&src, int width, int height)
{
  IMB_scale(src, width, height, IMBScaleFilter::Box, false);
}
static void imb_scale_box(ImBuf *&src, int width, int height)
{
  IMB_scale(src, width, height, IMBScaleFilter::Box, true);
}

static void scale_perf_impl(const char *name,
                            bool use_float,
                            void (*func)(ImBuf *&src, int width, int height))
{
  ImBuf *img = create_src_image(use_float);
  {
    SCOPED_TIMER(name);
    func(img, DST_LARGER_X, DST_LARGER_Y);
    func(img, SRC_X, SRC_Y);
    func(img, DST_SMALLER_X, DST_SMALLER_Y);
    func(img, DST_LARGER_X, DST_LARGER_Y);
  }
  IMB_freeImBuf(img);
}

static void test_scaling_perf(bool use_float)
{
  scale_perf_impl("scale_neare_s", use_float, imb_scale_nearest_st);
  scale_perf_impl("scale_neare_m", use_float, imb_scale_nearest);
  scale_perf_impl("xform_neare_m", use_float, imb_xform_nearest);

  scale_perf_impl("scale_bilin_s", use_float, imb_scale_bilinear_st);
  scale_perf_impl("scale_bilin_m", use_float, imb_scale_bilinear);
  scale_perf_impl("xform_bilin_m", use_float, imb_xform_bilinear);

  scale_perf_impl("scale_boxfl_s", use_float, imb_scale_box_st);
  scale_perf_impl("scale_boxfl_m", use_float, imb_scale_box);
  scale_perf_impl("xform_boxfl_m", use_float, imb_xform_box);
}

TEST(imbuf_scaling, scaling_perf_byte)
{
  test_scaling_perf(false);
}

TEST(imbuf_scaling, scaling_perf_float)
{
  test_scaling_perf(true);
}
