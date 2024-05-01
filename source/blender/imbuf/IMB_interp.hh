/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 *
 * Image buffer pixel sampling functions.
 * Mostly convenience wrappers around lower level `BLI_math_interp.hh`.
 */

#pragma once

#include "BLI_math_interp.hh"
#include "IMB_imbuf_types.hh"

#include <cstring>

namespace blender::imbuf {

/* Nearest sampling. */

[[nodiscard]] inline uchar4 interpolate_nearest_border_byte(const ImBuf *in, float u, float v)
{
  return math::interpolate_nearest_border_byte(in->byte_buffer.data, in->x, in->y, u, v);
}
[[nodiscard]] inline float4 interpolate_nearest_border_fl(const ImBuf *in, float u, float v)
{
  return math::interpolate_nearest_border_fl(in->float_buffer.data, in->x, in->y, u, v);
}
inline void interpolate_nearest_border_byte(const ImBuf *in, uchar output[4], float u, float v)
{
  math::interpolate_nearest_border_byte(in->byte_buffer.data, output, in->x, in->y, u, v);
}
inline void interpolate_nearest_border_fl(const ImBuf *in, float output[4], float u, float v)
{
  math::interpolate_nearest_border_fl(in->float_buffer.data, output, in->x, in->y, 4, u, v);
}

/* Nearest sampling with UV wrapping. */

[[nodiscard]] inline uchar4 interpolate_nearest_wrap_byte(const ImBuf *in, float u, float v)
{
  return math::interpolate_nearest_wrap_byte(in->byte_buffer.data, in->x, in->y, u, v);
}
[[nodiscard]] inline float4 interpolate_nearest_wrap_fl(const ImBuf *in, float u, float v)
{
  return math::interpolate_nearest_wrap_fl(in->float_buffer.data, in->x, in->y, u, v);
}

/* Bilinear sampling. */

[[nodiscard]] inline uchar4 interpolate_bilinear_byte(const ImBuf *in, float u, float v)
{
  return math::interpolate_bilinear_byte(in->byte_buffer.data, in->x, in->y, u, v);
}
[[nodiscard]] inline float4 interpolate_bilinear_fl(const ImBuf *in, float u, float v)
{
  return math::interpolate_bilinear_fl(in->float_buffer.data, in->x, in->y, u, v);
}
inline void interpolate_bilinear_byte(const ImBuf *in, uchar output[4], float u, float v)
{
  uchar4 col = math::interpolate_bilinear_byte(in->byte_buffer.data, in->x, in->y, u, v);
  memcpy(output, &col, sizeof(col));
}
inline void interpolate_bilinear_fl(const ImBuf *in, float output[4], float u, float v)
{
  float4 col = math::interpolate_bilinear_fl(in->float_buffer.data, in->x, in->y, u, v);
  memcpy(output, &col, sizeof(col));
}

/* Bilinear sampling, samples near edge blend into transparency. */

[[nodiscard]] inline uchar4 interpolate_bilinear_border_byte(const ImBuf *in, float u, float v)
{
  return math::interpolate_bilinear_border_byte(in->byte_buffer.data, in->x, in->y, u, v);
}
[[nodiscard]] inline float4 interpolate_bilinear_border_fl(const ImBuf *in, float u, float v)
{
  return math::interpolate_bilinear_border_fl(in->float_buffer.data, in->x, in->y, u, v);
}
inline void interpolate_bilinear_border_byte(const ImBuf *in, uchar output[4], float u, float v)
{
  uchar4 col = math::interpolate_bilinear_border_byte(in->byte_buffer.data, in->x, in->y, u, v);
  memcpy(output, &col, sizeof(col));
}
inline void interpolate_bilinear_border_fl(const ImBuf *in, float output[4], float u, float v)
{
  float4 col = math::interpolate_bilinear_border_fl(in->float_buffer.data, in->x, in->y, u, v);
  memcpy(output, &col, sizeof(col));
}

/* Bilinear sampling with UV wrapping. */

[[nodiscard]] inline uchar4 interpolate_bilinear_wrap_byte(const ImBuf *in, float u, float v)
{
  return math::interpolate_bilinear_wrap_byte(in->byte_buffer.data, in->x, in->y, u, v);
}
[[nodiscard]] inline float4 interpolate_bilinear_wrap_fl(const ImBuf *in, float u, float v)
{
  return math::interpolate_bilinear_wrap_fl(in->float_buffer.data, in->x, in->y, u, v);
}

/* Cubic B-Spline sampling. */

[[nodiscard]] inline uchar4 interpolate_cubic_bspline_byte(const ImBuf *in, float u, float v)
{
  return math::interpolate_cubic_bspline_byte(in->byte_buffer.data, in->x, in->y, u, v);
}
[[nodiscard]] inline float4 interpolate_cubic_bspline_fl(const ImBuf *in, float u, float v)
{
  return math::interpolate_cubic_bspline_fl(in->float_buffer.data, in->x, in->y, u, v);
}
inline void interpolate_cubic_bspline_byte(const ImBuf *in, uchar output[4], float u, float v)
{
  uchar4 col = math::interpolate_cubic_bspline_byte(in->byte_buffer.data, in->x, in->y, u, v);
  memcpy(output, &col, sizeof(col));
}
inline void interpolate_cubic_bspline_fl(const ImBuf *in, float output[4], float u, float v)
{
  float4 col = math::interpolate_cubic_bspline_fl(in->float_buffer.data, in->x, in->y, u, v);
  memcpy(output, &col, sizeof(col));
}

/* Cubic Mitchell sampling. */

[[nodiscard]] inline uchar4 interpolate_cubic_mitchell_byte(const ImBuf *in, float u, float v)
{
  return math::interpolate_cubic_mitchell_byte(in->byte_buffer.data, in->x, in->y, u, v);
}
inline void interpolate_cubic_mitchell_byte(const ImBuf *in, uchar output[4], float u, float v)
{
  uchar4 col = math::interpolate_cubic_mitchell_byte(in->byte_buffer.data, in->x, in->y, u, v);
  memcpy(output, &col, sizeof(col));
}

}  // namespace blender::imbuf

/**
 * Sample pixel of image using NEAREST method.
 */
void IMB_sampleImageAtLocation(
    ImBuf *ibuf, float x, float y, bool make_linear_rgb, float color[4]);
