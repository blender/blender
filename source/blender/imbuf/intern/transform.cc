/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
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
#include "BLI_vector.hh"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

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

  /* Per-subsample source image delta UVs. */
  Vector<float2, 9> subsampling_deltas;

  IndexRange dst_region_x_range;
  IndexRange dst_region_y_range;

  /* Cropping region in source image pixel space. */
  rctf src_crop;

  void init(const float4x4 &transform_matrix, const int num_subsamples, const bool has_source_crop)
  {
    start_uv = transform_matrix.location().xy();
    add_x = transform_matrix.x_axis().xy();
    add_y = transform_matrix.y_axis().xy();
    init_subsampling(num_subsamples);
    init_destination_region(transform_matrix, has_source_crop);
  }

 private:
  void init_subsampling(const int num_subsamples)
  {
    float2 subsample_add_x = add_x / num_subsamples;
    float2 subsample_add_y = add_y / num_subsamples;
    float2 offset_x = -add_x * 0.5f + subsample_add_x * 0.5f;
    float2 offset_y = -add_y * 0.5f + subsample_add_y * 0.5f;

    for (int y : IndexRange(0, num_subsamples)) {
      for (int x : IndexRange(0, num_subsamples)) {
        float2 delta_uv = offset_x + offset_y;
        delta_uv += x * subsample_add_x;
        delta_uv += y * subsample_add_y;
        subsampling_deltas.append(delta_uv);
      }
    }
  }

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
    for (const int2 &src_coords : {
             int2(src_crop.xmin, src_crop.ymin),
             int2(src_crop.xmax, src_crop.ymin),
             int2(src_crop.xmin, src_crop.ymax),
             int2(src_crop.xmax, src_crop.ymax),
         })
    {
      float3 dst_co = math::transform_point(inverse, float3(src_coords.x, src_coords.y, 0.0f));
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

template<int NumChannels>
static void sample_nearest_float(const ImBuf *source, float u, float v, float *r_sample)
{
  int x1 = int(u);
  int y1 = int(v);

  /* Break when sample outside image is requested. */
  if (x1 < 0 || x1 >= source->x || y1 < 0 || y1 >= source->y) {
    for (int i = 0; i < NumChannels; i++) {
      r_sample[i] = 0.0f;
    }
    return;
  }

  size_t offset = (size_t(source->x) * y1 + x1) * NumChannels;
  const float *dataF = source->float_buffer.data + offset;
  for (int i = 0; i < NumChannels; i++) {
    r_sample[i] = dataF[i];
  }
}

/* Read a pixel from an image buffer, with filtering/wrapping parameters. */
template<eIMBInterpolationFilterMode Filter, typename T, int NumChannels, bool WrapUV>
static void sample_image(const ImBuf *source, float u, float v, T *r_sample)
{
  if constexpr (WrapUV) {
    u = wrap_uv(u, source->x);
    v = wrap_uv(v, source->y);
  }
  /* BLI_bilinear_interpolation functions use `floor(uv)` and `floor(uv)+1`
   * texels. For proper mapping between pixel and texel spaces, need to
   * subtract 0.5. Same for bicubic. */
  if constexpr (Filter == IMB_FILTER_BILINEAR || Filter == IMB_FILTER_BICUBIC) {
    u -= 0.5f;
    v -= 0.5f;
  }
  if constexpr (Filter == IMB_FILTER_BILINEAR && std::is_same_v<T, float> && NumChannels == 4) {
    bilinear_interpolation_color_fl(source, r_sample, u, v);
  }
  else if constexpr (Filter == IMB_FILTER_NEAREST && std::is_same_v<T, uchar> && NumChannels == 4)
  {
    nearest_interpolation_color_char(source, r_sample, nullptr, u, v);
  }
  else if constexpr (Filter == IMB_FILTER_BILINEAR && std::is_same_v<T, uchar> && NumChannels == 4)
  {
    bilinear_interpolation_color_char(source, r_sample, u, v);
  }
  else if constexpr (Filter == IMB_FILTER_BILINEAR && std::is_same_v<T, float>) {
    if constexpr (WrapUV) {
      BLI_bilinear_interpolation_wrap_fl(source->float_buffer.data,
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
      BLI_bilinear_interpolation_fl(
          source->float_buffer.data, r_sample, source->x, source->y, NumChannels, u, v);
    }
  }
  else if constexpr (Filter == IMB_FILTER_NEAREST && std::is_same_v<T, float>) {
    sample_nearest_float<NumChannels>(source, u, v, r_sample);
  }
  else if constexpr (Filter == IMB_FILTER_BICUBIC && std::is_same_v<T, float>) {
    BLI_bicubic_interpolation_fl(
        source->float_buffer.data, r_sample, source->x, source->y, NumChannels, u, v);
  }
  else if constexpr (Filter == IMB_FILTER_BICUBIC && std::is_same_v<T, uchar> && NumChannels == 4)
  {
    BLI_bicubic_interpolation_char(source->byte_buffer.data, r_sample, source->x, source->y, u, v);
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
  /* Note: sample at pixel center for proper filtering. */
  float2 uv_start = ctx.start_uv + ctx.add_x * 0.5f + ctx.add_y * 0.5f;

  if (ctx.subsampling_deltas.size() > 1) {
    /* Multiple samples per pixel: accumulate them premultiplied,
     * divide by sample count and write out (un-premultiplying if writing out
     * to byte image). */
    const float inv_count = 1.0f / ctx.subsampling_deltas.size();
    for (int yi : y_range) {
      T *output = init_pixel_pointer<T>(ctx.dst, ctx.dst_region_x_range.first(), yi);
      float2 uv_row = uv_start + yi * ctx.add_y;
      for (int xi : ctx.dst_region_x_range) {
        float2 uv = uv_row + xi * ctx.add_x;
        float sample[4] = {};

        for (const float2 &delta_uv : ctx.subsampling_deltas) {
          const float2 sub_uv = uv + delta_uv;
          if (!CropSource || !should_discard(ctx, sub_uv)) {
            T sub_sample[4];
            sample_image<Filter, T, SrcChannels, WrapUV>(ctx.src, sub_uv.x, sub_uv.y, sub_sample);
            add_subsample(sub_sample, sample);
          }
        }

        mul_v4_v4fl(sample, sample, inv_count);
        store_premul_float_sample(sample, output);

        output += 4;
      }
    }
  }
  else {
    /* One sample per pixel. */
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
    /* Float images. */
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
  else if (ctx.dst->byte_buffer.data && ctx.src->byte_buffer.data) {
    /* Byte images. */
    if (channels == 4) {
      transform_scanlines<Filter, uchar, 4>(ctx, y_range);
    }
  }
}

}  // namespace blender::imbuf::transform

extern "C" {

using namespace blender::imbuf::transform;
using namespace blender;

void IMB_transform(const ImBuf *src,
                   ImBuf *dst,
                   const eIMBTransformMode mode,
                   const eIMBInterpolationFilterMode filter,
                   const int num_subsamples,
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
  ctx.init(blender::float4x4(transform_matrix), num_subsamples, crop);

  threading::parallel_for(ctx.dst_region_y_range, 8, [&](IndexRange y_range) {
    if (filter == IMB_FILTER_NEAREST) {
      transform_scanlines_filter<IMB_FILTER_NEAREST>(ctx, y_range);
    }
    else if (filter == IMB_FILTER_BILINEAR) {
      transform_scanlines_filter<IMB_FILTER_BILINEAR>(ctx, y_range);
    }
    else if (filter == IMB_FILTER_BICUBIC) {
      transform_scanlines_filter<IMB_FILTER_BICUBIC>(ctx, y_range);
    }
  });
}
}
