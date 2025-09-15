/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"
#include "gpu_shader_math_constants_lib.glsl"

/* [Drobot2014a] Low Level Optimizations for GCN. */
float sqrt_fast(float v)
{
  return intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(v) >> 1));
}
float2 sqrt_fast(float2 v)
{
  return intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(v) >> 1));
}

/* [Eberly2014] GPGPU Programming for Games and Science. */
float acos_fast(float v)
{
  float res = -0.156583f * abs(v) + M_PI_2;
  res *= sqrt_fast(1.0f - abs(v));
  return (v >= 0) ? res : M_PI - res;
}
float2 acos_fast(float2 v)
{
  float2 res = -0.156583f * abs(v) + M_PI_2;
  res *= sqrt_fast(1.0f - abs(v));
  v.x = (v.x >= 0) ? res.x : M_PI - res.x;
  v.y = (v.y >= 0) ? res.y : M_PI - res.y;
  return v;
}

float atan_fast(float x)
{
  float a = abs(x);
  float k = a > 1.0f ? (1.0f / a) : a;
  float s = 1.0f - (1.0f - k); /* Crush denormals. */
  float t = s * s;
  /* http://mathforum.org/library/drmath/view/62672.html
   * Examined 4278190080 values of atan:
   *   2.36864877 avg ULP diff, 302 max ULP, 6.55651e-06f max error      // (with  denormals)
   * Examined 4278190080 values of atan:
   *   171160502 avg ULP diff, 855638016 max ULP, 6.55651e-06f max error // (crush denormals)
   */
  float r = s * fma(0.43157974f, t, 1.0f) / fma(fma(0.05831938f, t, 0.76443945f), t, 1.0f);
  if (a > 1.0f) {
    r = 1.57079632679489661923f - r;
  }
  return (x < 0.0f) ? -r : r;
}
