/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* WORKAROUND: to guard against double include in EEVEE. */
#ifndef GPU_SHADER_MATH_FAST_LIB_GLSL
#define GPU_SHADER_MATH_FAST_LIB_GLSL

/* [Drobot2014a] Low Level Optimizations for GCN. */
float sqrt_fast(float v)
{
  return intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(v) >> 1));
}
vec2 sqrt_fast(vec2 v)
{
  return intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(v) >> 1));
}

/* [Eberly2014] GPGPU Programming for Games and Science. */
float acos_fast(float v)
{
  float res = -0.156583 * abs(v) + M_PI_2;
  res *= sqrt_fast(1.0 - abs(v));
  return (v >= 0) ? res : M_PI - res;
}
vec2 acos_fast(vec2 v)
{
  vec2 res = -0.156583 * abs(v) + M_PI_2;
  res *= sqrt_fast(1.0 - abs(v));
  v.x = (v.x >= 0) ? res.x : M_PI - res.x;
  v.y = (v.y >= 0) ? res.y : M_PI - res.y;
  return v;
}

float atan_fast(float x)
{
  float a = abs(x);
  float k = a > 1.0 ? (1.0 / a) : a;
  float s = 1.0 - (1.0 - k); /* Crush denormals. */
  float t = s * s;
  /* http://mathforum.org/library/drmath/view/62672.html
   * Examined 4278190080 values of atan:
   *   2.36864877 avg ULP diff, 302 max ULP, 6.55651e-06 max error      // (with  denormals)
   * Examined 4278190080 values of atan:
   *   171160502 avg ULP diff, 855638016 max ULP, 6.55651e-06 max error // (crush denormals)
   */
  float r = s * fma(0.43157974, t, 1.0) / fma(fma(0.05831938, t, 0.76443945), t, 1.0);
  if (a > 1.0) {
    r = 1.57079632679489661923 - r;
  }
  return (x < 0.0) ? -r : r;
}

#endif /* GPU_SHADER_MATH_FAST_LIB_GLSL */
