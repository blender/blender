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
 * Sampling completely outside the image returns transparent black.
 */

#include "BLI_math_base.h"
#include "BLI_math_vector_types.hh"

namespace blender::math {

/**
 * Nearest (point) sampling.
 *
 * Returns texel at floor(u,v) integer index -- note that it is not "nearest
 * to u,v coordinate", but rather with fractional part truncated (it would be
 * "nearest" if subtracting 0.5 from input u,v).
 */

inline void interpolate_nearest_byte(
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

[[nodiscard]] inline float4 interpolate_nearest_fl(
    const float *buffer, int width, int height, float u, float v)
{
  float4 res;
  interpolate_nearest_fl(buffer, res, width, height, 4, u, v);
  return res;
}

/**
 * Wrapped nearest sampling. (u,v) is repeated to be inside the image size.
 */

inline void interpolate_nearest_wrap_byte(
    const uchar *buffer, uchar *output, int width, int height, float u, float v)
{
  BLI_assert(buffer);
  u = floored_fmod(u, float(width));
  v = floored_fmod(v, float(height));
  int x = int(u);
  int y = int(v);
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
  u = floored_fmod(u, float(width));
  v = floored_fmod(v, float(height));
  int x = int(u);
  int y = int(v);
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

void interpolate_bilinear_wrap_fl(const float *buffer,
                                  float *output,
                                  int width,
                                  int height,
                                  int components,
                                  float u,
                                  float v,
                                  bool wrap_x,
                                  bool wrap_y);

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
