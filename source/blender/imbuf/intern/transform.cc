/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <type_traits>

#include "BLI_math_color_blend.h"
#include "BLI_math_interp.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_task.hh"

#include "IMB_imbuf.hh"
#include "IMB_interp.hh"

using blender::float4;
using blender::uchar4;

namespace blender::imbuf::transform {

struct TransformContext {
  const ImBuf *src;
  ImBuf *dst;
  eIMBTransformMode mode;

  /* UV coordinates at the destination origin (0,0) in source image space. */
  float2 start_uv;

  /* Source UV step delta, when moving along one destination pixel in X axis. */
  float2 add_x;

  /* Source UV step delta, when moving along one destination pixel in Y axis. */
  float2 add_y;

  /* Source corners in destination pixel space, counter-clockwise. */
  float2 src_corners[4];

  IndexRange dst_region_x_range;
  IndexRange dst_region_y_range;

  /* Cropping region in source image pixel space. */
  rctf src_crop;

  void init(const float4x4 &transform_matrix, const bool has_source_crop)
  {
    start_uv = transform_matrix.location().xy();
    add_x = transform_matrix.x_axis().xy();
    add_y = transform_matrix.y_axis().xy();
    init_destination_region(transform_matrix, has_source_crop);
  }

 private:
  void init_destination_region(const float4x4 &transform_matrix, const bool has_source_crop)
  {
    if (!has_source_crop) {
      dst_region_x_range = IndexRange(dst->x);
      dst_region_y_range = IndexRange(dst->y);
      return;
    }

    /* Transform the src_crop to the destination buffer with a margin. */
    const int2 margin(2);
    rcti rect;
    BLI_rcti_init_minmax(&rect);
    float4x4 inverse = math::invert(transform_matrix);
    const int2 src_coords[4] = {int2(src_crop.xmin, src_crop.ymin),
                                int2(src_crop.xmax, src_crop.ymin),
                                int2(src_crop.xmax, src_crop.ymax),
                                int2(src_crop.xmin, src_crop.ymax)};
    for (int i = 0; i < 4; i++) {
      int2 src_co = src_coords[i];
      float3 dst_co = math::transform_point(inverse, float3(src_co.x, src_co.y, 0.0f));
      src_corners[i] = float2(dst_co.x, dst_co.y);

      BLI_rcti_do_minmax_v(&rect, int2(dst_co) + margin);
      BLI_rcti_do_minmax_v(&rect, int2(dst_co) - margin);
    }

    /* Clamp rect to fit inside the image buffer. */
    rcti dest_rect;
    BLI_rcti_init(&dest_rect, 0, dst->x, 0, dst->y);
    BLI_rcti_isect(&rect, &dest_rect, &rect);
    dst_region_x_range = IndexRange(rect.xmin, BLI_rcti_size_x(&rect));
    dst_region_y_range = IndexRange(rect.ymin, BLI_rcti_size_y(&rect));
  }
};

/* Crop uv-coordinates that are outside the user data src_crop rect. */
static bool should_discard(const TransformContext &ctx, const float2 &uv)
{
  return uv.x < ctx.src_crop.xmin || uv.x >= ctx.src_crop.xmax || uv.y < ctx.src_crop.ymin ||
         uv.y >= ctx.src_crop.ymax;
}

template<typename T> static T *init_pixel_pointer(const ImBuf *image, int x, int y);
template<> uchar *init_pixel_pointer(const ImBuf *image, int x, int y)
{
  return image->byte_buffer.data + (size_t(y) * image->x + x) * image->channels;
}
template<> float *init_pixel_pointer(const ImBuf *image, int x, int y)
{
  return image->float_buffer.data + (size_t(y) * image->x + x) * image->channels;
}

static float wrap_uv(float value, int size)
{
  int x = int(floorf(value));
  if (UNLIKELY(x < 0 || x >= size)) {
    x %= size;
    if (x < 0) {
      x += size;
    }
  }
  return x;
}

/* Read a pixel from an image buffer, with filtering/wrapping parameters. */
template<eIMBInterpolationFilterMode Filter, typename T, int NumChannels, bool WrapUV>
static void sample_image(const ImBuf *source, float u, float v, T *r_sample)
{
  if constexpr (WrapUV) {
    u = wrap_uv(u, source->x);
    v = wrap_uv(v, source->y);
  }
  /* Bilinear/cubic interpolation functions use `floor(uv)` and `floor(uv)+1`
   * texels. For proper mapping between pixel and texel spaces, need to
   * subtract 0.5. */
  if constexpr (Filter != IMB_FILTER_NEAREST) {
    u -= 0.5f;
    v -= 0.5f;
  }
  if constexpr (Filter == IMB_FILTER_BILINEAR && std::is_same_v<T, float> && NumChannels == 4) {
    interpolate_bilinear_fl(source, r_sample, u, v);
  }
  else if constexpr (Filter == IMB_FILTER_NEAREST && std::is_same_v<T, uchar> && NumChannels == 4)
  {
    interpolate_nearest_border_byte(source, r_sample, u, v);
  }
  else if constexpr (Filter == IMB_FILTER_BILINEAR && std::is_same_v<T, uchar> && NumChannels == 4)
  {
    interpolate_bilinear_byte(source, r_sample, u, v);
  }
  else if constexpr (Filter == IMB_FILTER_BILINEAR && std::is_same_v<T, float>) {
    if constexpr (WrapUV) {
      math::interpolate_bilinear_wrap_fl(source->float_buffer.data,
                                         r_sample,
                                         source->x,
                                         source->y,
                                         NumChannels,
                                         u,
                                         v,
                                         true,
                                         true);
    }
    else {
      math::interpolate_bilinear_fl(
          source->float_buffer.data, r_sample, source->x, source->y, NumChannels, u, v);
    }
  }
  else if constexpr (Filter == IMB_FILTER_NEAREST && std::is_same_v<T, float>) {
    math::interpolate_nearest_border_fl(
        source->float_buffer.data, r_sample, source->x, source->y, NumChannels, u, v);
  }
  else if constexpr (Filter == IMB_FILTER_CUBIC_BSPLINE && std::is_same_v<T, float>) {
    math::interpolate_cubic_bspline_fl(
        source->float_buffer.data, r_sample, source->x, source->y, NumChannels, u, v);
  }
  else if constexpr (Filter == IMB_FILTER_CUBIC_BSPLINE && std::is_same_v<T, uchar> &&
                     NumChannels == 4)
  {
    interpolate_cubic_bspline_byte(source, r_sample, u, v);
  }
  else if constexpr (Filter == IMB_FILTER_CUBIC_MITCHELL && std::is_same_v<T, float>) {
    math::interpolate_cubic_mitchell_fl(
        source->float_buffer.data, r_sample, source->x, source->y, NumChannels, u, v);
  }
  else if constexpr (Filter == IMB_FILTER_CUBIC_MITCHELL && std::is_same_v<T, uchar> &&
                     NumChannels == 4)
  {
    interpolate_cubic_mitchell_byte(source, r_sample, u, v);
  }
  else {
    /* Unsupported sampler. */
    BLI_assert_unreachable();
  }
}

static void add_subsample(const float src[4], float dst[4])
{
  add_v4_v4(dst, src);
}

static void add_subsample(const uchar src[4], float dst[4])
{
  float premul[4];
  straight_uchar_to_premul_float(premul, src);
  add_v4_v4(dst, premul);
}

static void store_premul_float_sample(const float sample[4], float dst[4])
{
  copy_v4_v4(dst, sample);
}

static void store_premul_float_sample(const float sample[4], uchar dst[4])
{
  premul_float_to_straight_uchar(dst, sample);
}

template<int SrcChannels> static void store_sample(const uchar *sample, uchar *dst)
{
  BLI_STATIC_ASSERT(SrcChannels == 4, "Unsigned chars always have 4 channels.");
  copy_v4_v4_uchar(dst, sample);
}

template<int SrcChannels> static void store_sample(const float *sample, float *dst)
{
  if constexpr (SrcChannels == 4) {
    copy_v4_v4(dst, sample);
  }
  else if constexpr (SrcChannels == 3) {
    copy_v4_fl4(dst, sample[0], sample[1], sample[2], 1.0f);
  }
  else if constexpr (SrcChannels == 2) {
    copy_v4_fl4(dst, sample[0], sample[1], 0.0f, 1.0f);
  }
  else if constexpr (SrcChannels == 1) {
    /* Note: single channel sample is stored as grayscale. */
    copy_v4_fl4(dst, sample[0], sample[0], sample[0], 1.0f);
  }
  else {
    BLI_assert_unreachable();
  }
}

/* Process a block of destination image scanlines. */
template<eIMBInterpolationFilterMode Filter,
         typename T,
         int SrcChannels,
         bool CropSource,
         bool WrapUV>
static void process_scanlines(const TransformContext &ctx, IndexRange y_range)
{
  if constexpr (Filter == IMB_FILTER_BOX) {

    /* Multiple samples per pixel: accumulate them pre-multiplied,
     * divide by sample count and write out (un-pre-multiplying if writing out
     * to byte image).
     *
     * Do a box filter: for each destination pixel, accumulate XxY samples from source,
     * based on scaling factors (length of X/Y pixel steps). Use at least 2 samples
     * along each direction, so that in case of rotation the image gets
     * some anti-aliasing. Use at most 100 samples along each direction,
     * just as some way of clamping possible upper cost. Scaling something down by more
     * than 100x should rarely if ever happen, worst case they will get some aliasing.
     */
    float2 uv_start = ctx.start_uv;
    int sub_count_x = int(math::clamp(roundf(math::length(ctx.add_x)), 2.0f, 100.0f));
    int sub_count_y = int(math::clamp(roundf(math::length(ctx.add_y)), 2.0f, 100.0f));
    const float inv_count = 1.0f / (sub_count_x * sub_count_y);
    const float2 sub_step_x = ctx.add_x / sub_count_x;
    const float2 sub_step_y = ctx.add_y / sub_count_y;

    for (int yi : y_range) {
      T *output = init_pixel_pointer<T>(ctx.dst, ctx.dst_region_x_range.first(), yi);
      float2 uv_row = uv_start + yi * ctx.add_y;
      for (int xi : ctx.dst_region_x_range) {
        const float2 uv = uv_row + xi * ctx.add_x;
        float sample[4] = {};

        for (int sub_y = 0; sub_y < sub_count_y; sub_y++) {
          for (int sub_x = 0; sub_x < sub_count_x; sub_x++) {
            float2 delta = (sub_x + 0.5f) * sub_step_x + (sub_y + 0.5f) * sub_step_y;
            float2 sub_uv = uv + delta;
            if (!CropSource || !should_discard(ctx, sub_uv)) {
              T sub_sample[4];
              sample_image<eIMBInterpolationFilterMode::IMB_FILTER_NEAREST,
                           T,
                           SrcChannels,
                           WrapUV>(ctx.src, sub_uv.x, sub_uv.y, sub_sample);
              add_subsample(sub_sample, sample);
            }
          }
        }

        mul_v4_v4fl(sample, sample, inv_count);
        store_premul_float_sample(sample, output);

        output += 4;
      }
    }
  }
  else {
    /* One sample per pixel. Note: sample at pixel center for proper filtering. */
    float2 uv_start = ctx.start_uv + ctx.add_x * 0.5f + ctx.add_y * 0.5f;
    for (int yi : y_range) {
      T *output = init_pixel_pointer<T>(ctx.dst, ctx.dst_region_x_range.first(), yi);
      float2 uv_row = uv_start + yi * ctx.add_y;
      for (int xi : ctx.dst_region_x_range) {
        float2 uv = uv_row + xi * ctx.add_x;
        if (!CropSource || !should_discard(ctx, uv)) {
          T sample[4];
          sample_image<Filter, T, SrcChannels, WrapUV>(ctx.src, uv.x, uv.y, sample);
          store_sample<SrcChannels>(sample, output);
        }
        output += 4;
      }
    }
  }
}

template<eIMBInterpolationFilterMode Filter, typename T, int SrcChannels>
static void transform_scanlines(const TransformContext &ctx, IndexRange y_range)
{
  switch (ctx.mode) {
    case IMB_TRANSFORM_MODE_REGULAR:
      process_scanlines<Filter, T, SrcChannels, false, false>(ctx, y_range);
      break;
    case IMB_TRANSFORM_MODE_CROP_SRC:
      process_scanlines<Filter, T, SrcChannels, true, false>(ctx, y_range);
      break;
    case IMB_TRANSFORM_MODE_WRAP_REPEAT:
      process_scanlines<Filter, T, SrcChannels, false, true>(ctx, y_range);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

template<eIMBInterpolationFilterMode Filter>
static void transform_scanlines_filter(const TransformContext &ctx, IndexRange y_range)
{
  int channels = ctx.src->channels;

  if (ctx.dst->float_buffer.data && ctx.src->float_buffer.data) {
    /* Float pixels. */
    if (channels == 4) {
      transform_scanlines<Filter, float, 4>(ctx, y_range);
    }
    else if (channels == 3) {
      transform_scanlines<Filter, float, 3>(ctx, y_range);
    }
    else if (channels == 2) {
      transform_scanlines<Filter, float, 2>(ctx, y_range);
    }
    else if (channels == 1) {
      transform_scanlines<Filter, float, 1>(ctx, y_range);
    }
  }

  if (ctx.dst->byte_buffer.data && ctx.src->byte_buffer.data) {
    /* Byte pixels. */
    if (channels == 4) {
      transform_scanlines<Filter, uchar, 4>(ctx, y_range);
    }
  }
}

static float calc_coverage(float2 pos, int2 ipos, float2 delta, bool is_steep)
{
  /* Very approximate: just take difference from coordinate (x or y based on
   * steepness) to the integer coordinate. Adjust based on directions
   * of the edges. */
  float cov;
  if (is_steep) {
    cov = fabsf(ipos.x - pos.x);
    if (delta.y < 0) {
      cov = 1.0f - cov;
    }
  }
  else {
    cov = fabsf(ipos.y - pos.y);
    if (delta.x > 0) {
      cov = 1.0f - cov;
    }
  }
  cov = math::clamp(cov, 0.0f, 1.0f);
  /* Resulting coverage is 0.5 .. 1.0 range, since we are only covering
   * half of the pixels that should be AA'd (the other half is outside the
   * quad and does not get rasterized). Square the coverage to get
   * more range, and it looks a bit nicer that way. */
  cov *= cov;
  return cov;
}

static void edge_aa(const TransformContext &ctx)
{
  /* Rasterize along outer source edges into the destination image,
   * reducing alpha based on pixel distance to the edge at each pixel.
   * This is very approximate and not 100% correct "analytical AA",
   * but simple to do and better than nothing. */
  for (int line_idx = 0; line_idx < 4; line_idx++) {
    float2 ptA = ctx.src_corners[line_idx];
    float2 ptB = ctx.src_corners[(line_idx + 1) & 3];
    float2 delta = ptB - ptA;
    float2 abs_delta = math::abs(delta);
    float length = math::max(abs_delta.x, abs_delta.y);
    if (length < 1) {
      continue;
    }
    bool is_steep = length == abs_delta.y;

    /* It is very common to have non-rotated strips; check if edge line is
     * horizontal or vertical and would not alter the coverage and can
     * be skipped. */
    constexpr float NO_ROTATION = 1.0e-6f;
    constexpr float NO_AA_CONTRIB = 1.0e-2f;
    if (is_steep) {
      if ((abs_delta.x < NO_ROTATION) && (fabsf(ptA.x - roundf(ptA.x)) < NO_AA_CONTRIB)) {
        continue;
      }
    }
    else {
      if ((abs_delta.y < NO_ROTATION) && (fabsf(ptA.y - roundf(ptA.y)) < NO_AA_CONTRIB)) {
        continue;
      }
    }

    /* DDA line raster: step one pixel along the longer direction. */
    delta /= length;
    if (ctx.dst->float_buffer.data != nullptr) {
      /* Float pixels. */
      float *dst = ctx.dst->float_buffer.data;
      for (int i = 0; i < length; i++) {
        float2 pos = ptA + i * delta;
        int2 ipos = int2(pos);
        if (ipos.x >= 0 && ipos.x < ctx.dst->x && ipos.y >= 0 && ipos.y < ctx.dst->y) {
          float cov = calc_coverage(pos, ipos, delta, is_steep);
          size_t idx = (size_t(ipos.y) * ctx.dst->x + ipos.x) * 4;
          dst[idx + 0] *= cov;
          dst[idx + 1] *= cov;
          dst[idx + 2] *= cov;
          dst[idx + 3] *= cov;
        }
      }
    }
    if (ctx.dst->byte_buffer.data != nullptr) {
      /* Byte pixels. */
      uchar *dst = ctx.dst->byte_buffer.data;
      for (int i = 0; i < length; i++) {
        float2 pos = ptA + i * delta;
        int2 ipos = int2(pos);
        if (ipos.x >= 0 && ipos.x < ctx.dst->x && ipos.y >= 0 && ipos.y < ctx.dst->y) {
          float cov = calc_coverage(pos, ipos, delta, is_steep);
          size_t idx = (size_t(ipos.y) * ctx.dst->x + ipos.x) * 4;
          dst[idx + 3] *= cov;
        }
      }
    }
  }
}

}  // namespace blender::imbuf::transform

using namespace blender::imbuf::transform;
using namespace blender;

void IMB_transform(const ImBuf *src,
                   ImBuf *dst,
                   const eIMBTransformMode mode,
                   const eIMBInterpolationFilterMode filter,
                   const float transform_matrix[4][4],
                   const rctf *src_crop)
{
  BLI_assert_msg(mode != IMB_TRANSFORM_MODE_CROP_SRC || src_crop != nullptr,
                 "No source crop rect given, but crop source is requested. Or source crop rect "
                 "was given, but crop source was not requested.");
  BLI_assert_msg(dst->channels == 4, "Destination image must have 4 channels.");

  TransformContext ctx;
  ctx.src = src;
  ctx.dst = dst;
  ctx.mode = mode;
  bool crop = mode == IMB_TRANSFORM_MODE_CROP_SRC;
  if (crop) {
    ctx.src_crop = *src_crop;
  }
  ctx.init(blender::float4x4(transform_matrix), crop);

  threading::parallel_for(ctx.dst_region_y_range, 8, [&](IndexRange y_range) {
    if (filter == IMB_FILTER_NEAREST) {
      transform_scanlines_filter<IMB_FILTER_NEAREST>(ctx, y_range);
    }
    else if (filter == IMB_FILTER_BILINEAR) {
      transform_scanlines_filter<IMB_FILTER_BILINEAR>(ctx, y_range);
    }
    else if (filter == IMB_FILTER_CUBIC_BSPLINE) {
      transform_scanlines_filter<IMB_FILTER_CUBIC_BSPLINE>(ctx, y_range);
    }
    else if (filter == IMB_FILTER_CUBIC_MITCHELL) {
      transform_scanlines_filter<IMB_FILTER_CUBIC_MITCHELL>(ctx, y_range);
    }
    else if (filter == IMB_FILTER_BOX) {
      transform_scanlines_filter<IMB_FILTER_BOX>(ctx, y_range);
    }
  });

  if (crop && (filter != IMB_FILTER_NEAREST)) {
    edge_aa(ctx);
  }
}
