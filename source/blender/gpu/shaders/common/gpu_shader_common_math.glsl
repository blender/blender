/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"

[[node]]
void math_add(float a, float b, float c, float &result)
{
  result = a + b;
}

[[node]]
void math_subtract(float a, float b, float c, float &result)
{
  result = a - b;
}

[[node]]
void math_multiply(float a, float b, float c, float &result)
{
  result = a * b;
}

[[node]]
void math_divide(float a, float b, float c, float &result)
{
  result = safe_divide(a, b);
}

[[node]]
void math_power(float a, float b, float c, float &result)
{
  if (a >= 0.0f) {
    result = compatible_pow(a, b);
  }
  else {
    float fraction = mod(abs(b), 1.0f);
    if (fraction > 0.999f || fraction < 0.001f) {
      result = compatible_pow(a, floor(b + 0.5f));
    }
    else {
      result = 0.0f;
    }
  }
}

[[node]]
void math_logarithm(float a, float b, float c, float &result)
{
  result = (a > 0.0f && b > 0.0f) ? log2(a) / log2(b) : 0.0f;
}

[[node]]
void math_sqrt(float a, float b, float c, float &result)
{
  result = (a > 0.0f) ? sqrt(a) : 0.0f;
}

[[node]]
void math_inversesqrt(float a, float b, float c, float &result)
{
  result = inversesqrt(a);
}

[[node]]
void math_absolute(float a, float b, float c, float &result)
{
  result = abs(a);
}

[[node]]
void math_radians(float a, float b, float c, float &result)
{
  result = radians(a);
}

[[node]]
void math_degrees(float a, float b, float c, float &result)
{
  result = degrees(a);
}

[[node]]
void math_minimum(float a, float b, float c, float &result)
{
  result = min(a, b);
}

[[node]]
void math_maximum(float a, float b, float c, float &result)
{
  result = max(a, b);
}

[[node]]
void math_less_than(float a, float b, float c, float &result)
{
  result = (a < b) ? 1.0f : 0.0f;
}

[[node]]
void math_greater_than(float a, float b, float c, float &result)
{
  result = (a > b) ? 1.0f : 0.0f;
}

[[node]]
void math_round(float a, float b, float c, float &result)
{
  result = floor(a + 0.5f);
}

[[node]]
void math_floor(float a, float b, float c, float &result)
{
  result = floor(a);
}

[[node]]
void math_ceil(float a, float b, float c, float &result)
{
  result = ceil(a);
}

[[node]]
void math_fraction(float a, float b, float c, float &result)
{
  result = a - floor(a);
}

[[node]]
void math_modulo(float a, float b, float c, float &result)
{
  result = compatible_mod(a, b);
}

[[node]]
void math_floored_modulo(float a, float b, float c, float &result)
{
  result = (b != 0.0f) ? a - floor(a / b) * b : 0.0f;
}

[[node]]
void math_trunc(float a, float b, float c, float &result)
{
  result = trunc(a);
}

[[node]]
void math_snap(float a, float b, float c, float &result)
{
  result = floor(safe_divide(a, b)) * b;
}

[[node]]
void math_pingpong(float a, float b, float c, float &result)
{
  result = (b != 0.0f) ? abs(fract((a - b) / (b * 2.0f)) * b * 2.0f - b) : 0.0f;
}

/* Adapted from GODOT-engine math_funcs.h. */
[[node]]
void math_wrap(float a, float b, float c, float &result)
{
  result = wrap(a, b, c);
}

[[node]]
void math_sine(float a, float b, float c, float &result)
{
  result = sin(a);
}

[[node]]
void math_cosine(float a, float b, float c, float &result)
{
  result = cos(a);
}

[[node]]
void math_tangent(float a, float b, float c, float &result)
{
  result = tan(a);
}

[[node]]
void math_sinh(float a, float b, float c, float &result)
{
  result = sinh(a);
}

[[node]]
void math_cosh(float a, float b, float c, float &result)
{
  result = cosh(a);
}

[[node]]
void math_tanh(float a, float b, float c, float &result)
{
  result = tanh(a);
}

[[node]]
void math_arcsine(float a, float b, float c, float &result)
{
  result = (a <= 1.0f && a >= -1.0f) ? asin(a) : 0.0f;
}

[[node]]
void math_arccosine(float a, float b, float c, float &result)
{
  result = (a <= 1.0f && a >= -1.0f) ? acos(a) : 0.0f;
}

[[node]]
void math_arctangent(float a, float b, float c, float &result)
{
  result = atan(a);
}

/* The behavior of `atan2(0, 0)` is undefined on many platforms, to ensure consistent behavior, we
 * return 0 in this case. See !126951. */
[[node]]
void math_arctan2(float a, float b, float c, float &result)
{
  result = ((a == 0.0f && b == 0.0f) ? 0.0f : atan(a, b));
}

[[node]]
void math_sign(float a, float b, float c, float &result)
{
  result = sign(a);
}

[[node]]
void math_exponent(float a, float b, float c, float &result)
{
  result = exp(a);
}

[[node]]
void math_compare(float a, float b, float c, float &result)
{
  result = (abs(a - b) <= max(c, 1e-5f)) ? 1.0f : 0.0f;
}

[[node]]
void math_multiply_add(float a, float b, float c, float &result)
{
  result = a * b + c;
}

/* See: https://www.iquilezles.org/www/articles/smin/smin.htm. */
[[node]]
void math_smoothmin(float a, float b, float c, float &result)
{
  if (c != 0.0f) {
    float h = max(c - abs(a - b), 0.0f) / c;
    result = min(a, b) - h * h * h * c * (1.0f / 6.0f);
  }
  else {
    result = min(a, b);
  }
}

[[node]]
void math_smoothmax(float a, float b, float c, float &result)
{
  math_smoothmin(-a, -b, c, result);
  result = -result;
}

/* TODO(fclem): Fix dependency hell one EEVEE legacy is removed. */
float math_reduce_max(float3 a)
{
  return max(a.x, max(a.y, a.z));
}

float math_average(float3 a)
{
  return (a.x + a.y + a.z) * (1.0f / 3.0f);
}
