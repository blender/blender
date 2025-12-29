/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * 2D image sampling with filtering functions.
 *
 * All functions take (u, v) texture coordinate, non-normalized (i.e. ranging
 * from (0,0) to (width,height) over the image).
 *
 * Any filtering done on texel values just blends them without color space or
 * gamma conversions.
 *
 * For sampling float images, there are "fully generic" functions that
 * take arbitrary image channel counts, and arbitrary texture coordinate wrapping
 * modes. However if you do not need full flexibility, use less generic functions,
 * they will be faster (e.g. #interpolate_nearest_border_fl is faster than
 * #interpolate_nearest_wrapmode_fl).
 */

#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_simd.hh"

namespace blender::math {

/**
 * Texture coordinate wrapping mode.
 */
enum class InterpWrapMode {
  /** Image edges are extended outside the image, i.e. sample coordinates are clamped to the edge.
   */
  Extend,
  /** Image repeats, i.e. sample coordinates are wrapped around. */
  Repeat,
  /** Samples outside the image return transparent black. */
  Border
};

BLI_INLINE int32_t wrap_coord(float u, int32_t size, InterpWrapMode wrap)
{
  if (u >= 0) {
    if (u < float(size)) {
      return int32_t(u);
    }
    switch (wrap) {
      default: /* case InterpWrapMode::Extend: */
        return size - 1;
      case InterpWrapMode::Repeat:
        return int32_t(uint32_t(u) % uint32_t(size));
      case InterpWrapMode::Border:
        return -1;
    }
  }
  switch (wrap) {
    default: /* case InterpWrapMode::Extend: */
      return 0;
    case InterpWrapMode::Repeat: {
      int32_t x = int32_t(uint32_t(-floorf(u)) % uint32_t(size));
      return x ? size - x : 0;
    }
    case InterpWrapMode::Border:
      return -1;
  }
}

/* -------------------------------------------------------------------- */
/* Nearest (point) sampling. */

BLI_INLINE void interpolate_nearest_wrapmode_fl(const float *buffer,
                                                float *output,
                                                int width,
                                                int height,
                                                int components,
                                                float u,
                                                float v,
                                                InterpWrapMode wrap_u,
                                                InterpWrapMode wrap_v)
{
  BLI_assert(buffer);
  int x = wrap_coord(u, width, wrap_u);
  int y = wrap_coord(v, height, wrap_v);
  if (x < 0 || y < 0) {
    for (int i = 0; i < components; i++) {
      output[i] = 0.0f;
    }
    return;
  }

  const float *data = buffer + (int64_t(width) * y + x) * components;
  for (int i = 0; i < components; i++) {
    output[i] = data[i];
  }
}

/**
 * Nearest (point) sampling (with black border).
 *
 * Returns texel at floor(u,v) integer index. Samples outside the image are turned into transparent
 * black.
 *
 * Note that it is not "nearest to u,v coordinate", but rather with fractional part truncated (it
 * would be "nearest" if subtracting 0.5 from input u,v).
 */

inline void interpolate_nearest_border_byte(
    const uchar *buffer, uchar *output, int width, int height, float u, float v)
{
  BLI_assert(buffer);
  int x = int(u);
  int y = int(v);

  /* Outside image? */
  if (x < 0 || x >= width || y < 0 || y >= height) {
    output[0] = output[1] = output[2] = output[3] = 0;
    return;
  }

  const uchar *data = buffer + (int64_t(width) * y + x) * 4;
  output[0] = data[0];
  output[1] = data[1];
  output[2] = data[2];
  output[3] = data[3];
}

[[nodiscard]] inline uchar4 interpolate_nearest_border_byte(
    const uchar *buffer, int width, int height, float u, float v)
{
  uchar4 res;
  interpolate_nearest_border_byte(buffer, res, width, height, u, v);
  return res;
}

inline void interpolate_nearest_border_fl(
    const float *buffer, float *output, int width, int height, int components, float u, float v)
{
  BLI_assert(buffer);
  int x = int(u);
  int y = int(v);

  /* Outside image? */
  if (x < 0 || x >= width || y < 0 || y >= height) {
    for (int i = 0; i < components; i++) {
      output[i] = 0.0f;
    }
    return;
  }

  const float *data = buffer + (int64_t(width) * y + x) * components;
  for (int i = 0; i < components; i++) {
    output[i] = data[i];
  }
}

[[nodiscard]] inline float4 interpolate_nearest_border_fl(
    const float *buffer, int width, int height, float u, float v)
{
  float4 res;
  interpolate_nearest_border_fl(buffer, res, width, height, 4, u, v);
  return res;
}

/**
 * Nearest (point) sampling.
 *
 * Returns texel at floor(u,v) integer index. Samples outside the image are clamped to texels at
 * image edge.
 *
 * Note that it is not "nearest to u,v coordinate", but rather with fractional part truncated (it
 * would be "nearest" if subtracting 0.5 from input u,v).
 */

inline void interpolate_nearest_byte(
    const uchar *buffer, uchar *output, int width, int height, float u, float v)
{
  BLI_assert(buffer);
  const int x = u > 0 ? (u < width ? int(u) : width - 1) : 0;
  const int y = v > 0 ? (v < height ? int(v) : height - 1) : 0;

  const uchar *data = buffer + (int64_t(width) * y + x) * 4;
  output[0] = data[0];
  output[1] = data[1];
  output[2] = data[2];
  output[3] = data[3];
}

[[nodiscard]] inline uchar4 interpolate_nearest_byte(
    const uchar *buffer, int width, int height, float u, float v)
{
  uchar4 res;
  interpolate_nearest_byte(buffer, res, width, height, u, v);
  return res;
}

inline void interpolate_nearest_fl(
    const float *buffer, float *output, int width, int height, int components, float u, float v)
{
  BLI_assert(buffer);
  const int x = u > 0 ? (u < width ? int(u) : width - 1) : 0;
  const int y = v > 0 ? (v < height ? int(v) : height - 1) : 0;

  const float *data = buffer + (int64_t(width) * y + x) * components;
  for (int i = 0; i < components; i++) {
    output[i] = data[i];
  }
}

[[nodiscard]] inline float4 interpolate_nearest_fl(
    const float *buffer, int width, int height, float u, float v)
{
  float4 res;
  interpolate_nearest_fl(buffer, res, width, height, 4, u, v);
  return res;
}

/**
 * Equal to int(mod_periodic(u, float(size)) for |u| <= MAXINT.
 * However other values of u, including inf and NaN, produce in-range values,
 * this is also at least 5% faster.
 */
[[nodiscard]] inline int32_t wrap_coord(float u, int32_t size)
{
  if (u < 0) {
    int32_t x = int(uint32_t(-floor(u)) % uint32_t(size));
    return x ? size - x : 0;
  }
  return int(uint32_t(u) % uint32_t(size));
}

/**
 * Wrapped nearest sampling. (u,v) is repeated to be inside the image size.
 */

inline void interpolate_nearest_wrap_byte(
    const uchar *buffer, uchar *output, int width, int height, float u, float v)
{
  BLI_assert(buffer);
  int x = wrap_coord(u, width);
  int y = wrap_coord(v, height);
  BLI_assert(x >= 0 && y >= 0 && x < width && y < height);

  const uchar *data = buffer + (int64_t(width) * y + x) * 4;
  output[0] = data[0];
  output[1] = data[1];
  output[2] = data[2];
  output[3] = data[3];
}

[[nodiscard]] inline uchar4 interpolate_nearest_wrap_byte(
    const uchar *buffer, int width, int height, float u, float v)
{
  uchar4 res;
  interpolate_nearest_wrap_byte(buffer, res, width, height, u, v);
  return res;
}

inline void interpolate_nearest_wrap_fl(
    const float *buffer, float *output, int width, int height, int components, float u, float v)
{
  BLI_assert(buffer);
  int x = wrap_coord(u, width);
  int y = wrap_coord(v, height);
  BLI_assert(x >= 0 && y >= 0 && x < width && y < height);

  const float *data = buffer + (int64_t(width) * y + x) * components;
  for (int i = 0; i < components; i++) {
    output[i] = data[i];
  }
}

[[nodiscard]] inline float4 interpolate_nearest_wrap_fl(
    const float *buffer, int width, int height, float u, float v)
{
  float4 res;
  interpolate_nearest_wrap_fl(buffer, res, width, height, 4, u, v);
  return res;
}

/* -------------------------------------------------------------------- */
/* Bilinear sampling. */

BLI_INLINE void interpolate_bilinear_wrapmode_fl(const float *buffer,
                                                 float *output,
                                                 int width,
                                                 int height,
                                                 int components,
                                                 float u,
                                                 float v,
                                                 InterpWrapMode wrap_x,
                                                 InterpWrapMode wrap_y)
{
  BLI_assert(buffer && output);
  BLI_assert(components > 0 && components <= 4);

  int x1 = wrap_coord(u, width, wrap_x);
  int x2 = wrap_coord(u + 1, width, wrap_x);
  int y1 = wrap_coord(v, height, wrap_y);
  int y2 = wrap_coord(v + 1, height, wrap_y);

  const float *row1, *row2, *row3, *row4;
  const float empty[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  row1 = buffer + (int64_t(width) * y1 + x1) * components;
  row2 = buffer + (int64_t(width) * y2 + x1) * components;
  row3 = buffer + (int64_t(width) * y1 + x2) * components;
  row4 = buffer + (int64_t(width) * y2 + x2) * components;

  if (wrap_x == InterpWrapMode::Border) {
    if (x1 < 0) {
      row1 = empty;
      row2 = empty;
    }
    if (x2 < 0) {
      row3 = empty;
      row4 = empty;
    }
  }
  if (wrap_y == InterpWrapMode::Border) {
    if (y1 < 0) {
      row1 = empty;
      row3 = empty;
    }
    if (y2 < 0) {
      row2 = empty;
      row4 = empty;
    }
  }

  /* Finally, do interpolation. */
  float a = u - floorf(u);
  float b = v - floorf(v);
  float a_b = a * b;
  float ma_b = (1.0f - a) * b;
  float a_mb = a * (1.0f - b);
  float ma_mb = (1.0f - a) * (1.0f - b);

  if (components == 1) {
    output[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
  }
  else if (components == 2) {
    output[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
    output[1] = ma_mb * row1[1] + a_mb * row3[1] + ma_b * row2[1] + a_b * row4[1];
  }
  else if (components == 3) {
    output[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
    output[1] = ma_mb * row1[1] + a_mb * row3[1] + ma_b * row2[1] + a_b * row4[1];
    output[2] = ma_mb * row1[2] + a_mb * row3[2] + ma_b * row2[2] + a_b * row4[2];
  }
  else {
#if BLI_HAVE_SSE2
    __m128 rgba1 = _mm_loadu_ps(row1);
    __m128 rgba2 = _mm_loadu_ps(row2);
    __m128 rgba3 = _mm_loadu_ps(row3);
    __m128 rgba4 = _mm_loadu_ps(row4);
    rgba1 = _mm_mul_ps(_mm_set1_ps(ma_mb), rgba1);
    rgba2 = _mm_mul_ps(_mm_set1_ps(ma_b), rgba2);
    rgba3 = _mm_mul_ps(_mm_set1_ps(a_mb), rgba3);
    rgba4 = _mm_mul_ps(_mm_set1_ps(a_b), rgba4);
    __m128 rgba13 = _mm_add_ps(rgba1, rgba3);
    __m128 rgba24 = _mm_add_ps(rgba2, rgba4);
    __m128 rgba = _mm_add_ps(rgba13, rgba24);
    _mm_storeu_ps(output, rgba);
#else
    output[0] = ma_mb * row1[0] + a_mb * row3[0] + ma_b * row2[0] + a_b * row4[0];
    output[1] = ma_mb * row1[1] + a_mb * row3[1] + ma_b * row2[1] + a_b * row4[1];
    output[2] = ma_mb * row1[2] + a_mb * row3[2] + ma_b * row2[2] + a_b * row4[2];
    output[3] = ma_mb * row1[3] + a_mb * row3[3] + ma_b * row2[3] + a_b * row4[3];
#endif
  }
}

/**
 * Bilinear sampling (with black border).
 *
 * Takes four image samples at floor(u,v) and floor(u,v)+1, and blends them
 * based on fractional parts of u,v. Samples outside the image are turned
 * into transparent black.
 *
 * Note that you probably want to subtract 0.5 from u,v before this function,
 * to get proper filtering.
 */

[[nodiscard]] uchar4 interpolate_bilinear_border_byte(
    const uchar *buffer, int width, int height, float u, float v);

[[nodiscard]] float4 interpolate_bilinear_border_fl(
    const float *buffer, int width, int height, float u, float v);

void interpolate_bilinear_border_fl(
    const float *buffer, float *output, int width, int height, int components, float u, float v);

/**
 * Bilinear sampling.
 *
 * Takes four image samples at floor(u,v) and floor(u,v)+1, and blends them
 * based on fractional parts of u,v.
 * Samples outside the image are clamped to texels at image edge.
 *
 * Note that you probably want to subtract 0.5 from u,v before this function,
 * to get proper filtering.
 */

[[nodiscard]] uchar4 interpolate_bilinear_byte(
    const uchar *buffer, int width, int height, float u, float v);

[[nodiscard]] float4 interpolate_bilinear_fl(
    const float *buffer, int width, int height, float u, float v);

void interpolate_bilinear_fl(
    const float *buffer, float *output, int width, int height, int components, float u, float v);

/**
 * Wrapped bilinear sampling. (u,v) is repeated to be inside the image size,
 * including properly wrapping samples that are right on the edges.
 */

[[nodiscard]] uchar4 interpolate_bilinear_wrap_byte(
    const uchar *buffer, int width, int height, float u, float v);

[[nodiscard]] float4 interpolate_bilinear_wrap_fl(
    const float *buffer, int width, int height, float u, float v);

/* -------------------------------------------------------------------- */
/* Cubic sampling. */

/**
 * Cubic B-Spline sampling.
 *
 * Takes 4x4 image samples at floor(u,v)-1 .. floor(u,v)+2, and blends them
 * based on fractional parts of u,v. Uses B-Spline variant Mitchell-Netravali
 * filter (B=1, C=0), which has no ringing but introduces quite a lot of blur.
 * Samples outside the image are clamped to texels at image edge.
 *
 * Note that you probably want to subtract 0.5 from u,v before this function,
 * to get proper filtering.
 */

[[nodiscard]] uchar4 interpolate_cubic_bspline_byte(
    const uchar *buffer, int width, int height, float u, float v);

[[nodiscard]] float4 interpolate_cubic_bspline_fl(
    const float *buffer, int width, int height, float u, float v);

void interpolate_cubic_bspline_fl(
    const float *buffer, float *output, int width, int height, int components, float u, float v);

void interpolate_cubic_bspline_wrapmode_fl(const float *buffer,
                                           float *output,
                                           int width,
                                           int height,
                                           int components,
                                           float u,
                                           float v,
                                           InterpWrapMode wrap_u,
                                           InterpWrapMode wrap_v);

/**
 * Cubic Mitchell sampling.
 *
 * Takes 4x4 image samples at floor(u,v)-1 .. floor(u,v)+2, and blends them
 * based on fractional parts of u,v. Uses Mitchell-Netravali filter (B=C=1/3),
 * which has a good compromise between blur and ringing.
 * Samples outside the image are clamped to texels at image edge.
 *
 * Note that you probably want to subtract 0.5 from u,v before this function,
 * to get proper filtering.
 */

[[nodiscard]] uchar4 interpolate_cubic_mitchell_byte(
    const uchar *buffer, int width, int height, float u, float v);

[[nodiscard]] float4 interpolate_cubic_mitchell_fl(
    const float *buffer, int width, int height, float u, float v);

void interpolate_cubic_mitchell_fl(
    const float *buffer, float *output, int width, int height, int components, float u, float v);

}  // namespace blender::math

/* -------------------------------------------------------------------- */
/* EWA sampling. */

#define EWA_MAXIDX 255
extern const float EWA_WTS[EWA_MAXIDX + 1];

using ewa_filter_read_pixel_cb = void (*)(void *userdata, int x, int y, float result[4]);

void BLI_ewa_imp2radangle(
    float A, float B, float C, float F, float *a, float *b, float *th, float *ecc);

/**
 * TODO(sergey): Consider making this function inlined, so the pixel read callback
 * could also be inlined in order to avoid per-pixel function calls.
 */
void BLI_ewa_filter(int width,
                    int height,
                    bool intpol,
                    bool use_alpha,
                    const float uv[2],
                    const float du[2],
                    const float dv[2],
                    ewa_filter_read_pixel_cb read_pixel_cb,
                    void *userdata,
                    float result[4]);
