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

#endif /* GPU_SHADER_MATH_FAST_LIB_GLSL */
