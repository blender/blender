/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)

void math_add(float a, float b, float c, out float result)
{
  result = a + b;
}

void math_subtract(float a, float b, float c, out float result)
{
  result = a - b;
}

void math_multiply(float a, float b, float c, out float result)
{
  result = a * b;
}

void math_divide(float a, float b, float c, out float result)
{
  result = safe_divide(a, b);
}

void math_power(float a, float b, float c, out float result)
{
  if (a >= 0.0) {
    result = compatible_pow(a, b);
  }
  else {
    float fraction = mod(abs(b), 1.0);
    if (fraction > 0.999 || fraction < 0.001) {
      result = compatible_pow(a, floor(b + 0.5));
    }
    else {
      result = 0.0;
    }
  }
}

void math_logarithm(float a, float b, float c, out float result)
{
  result = (a > 0.0 && b > 0.0) ? log2(a) / log2(b) : 0.0;
}

void math_sqrt(float a, float b, float c, out float result)
{
  result = (a > 0.0) ? sqrt(a) : 0.0;
}

void math_inversesqrt(float a, float b, float c, out float result)
{
  result = inversesqrt(a);
}

void math_absolute(float a, float b, float c, out float result)
{
  result = abs(a);
}

void math_radians(float a, float b, float c, out float result)
{
  result = radians(a);
}

void math_degrees(float a, float b, float c, out float result)
{
  result = degrees(a);
}

void math_minimum(float a, float b, float c, out float result)
{
  result = min(a, b);
}

void math_maximum(float a, float b, float c, out float result)
{
  result = max(a, b);
}

void math_less_than(float a, float b, float c, out float result)
{
  result = (a < b) ? 1.0 : 0.0;
}

void math_greater_than(float a, float b, float c, out float result)
{
  result = (a > b) ? 1.0 : 0.0;
}

void math_round(float a, float b, float c, out float result)
{
  result = floor(a + 0.5);
}

void math_floor(float a, float b, float c, out float result)
{
  result = floor(a);
}

void math_ceil(float a, float b, float c, out float result)
{
  result = ceil(a);
}

void math_fraction(float a, float b, float c, out float result)
{
  result = a - floor(a);
}

void math_modulo(float a, float b, float c, out float result)
{
  result = compatible_fmod(a, b);
}

void math_floored_modulo(float a, float b, float c, out float result)
{
  result = (b != 0.0) ? a - floor(a / b) * b : 0.0;
}

void math_trunc(float a, float b, float c, out float result)
{
  result = trunc(a);
}

void math_snap(float a, float b, float c, out float result)
{
  result = floor(safe_divide(a, b)) * b;
}

void math_pingpong(float a, float b, float c, out float result)
{
  result = (b != 0.0) ? abs(fract((a - b) / (b * 2.0)) * b * 2.0 - b) : 0.0;
}

/* Adapted from GODOT-engine math_funcs.h. */
void math_wrap(float a, float b, float c, out float result)
{
  result = wrap(a, b, c);
}

void math_sine(float a, float b, float c, out float result)
{
  result = sin(a);
}

void math_cosine(float a, float b, float c, out float result)
{
  result = cos(a);
}

void math_tangent(float a, float b, float c, out float result)
{
  result = tan(a);
}

void math_sinh(float a, float b, float c, out float result)
{
  result = sinh(a);
}

void math_cosh(float a, float b, float c, out float result)
{
  result = cosh(a);
}

void math_tanh(float a, float b, float c, out float result)
{
  result = tanh(a);
}

void math_arcsine(float a, float b, float c, out float result)
{
  result = (a <= 1.0 && a >= -1.0) ? asin(a) : 0.0;
}

void math_arccosine(float a, float b, float c, out float result)
{
  result = (a <= 1.0 && a >= -1.0) ? acos(a) : 0.0;
}

void math_arctangent(float a, float b, float c, out float result)
{
  result = atan(a);
}

void math_arctan2(float a, float b, float c, out float result)
{
  result = atan(a, b);
}

void math_sign(float a, float b, float c, out float result)
{
  result = sign(a);
}

void math_exponent(float a, float b, float c, out float result)
{
  result = exp(a);
}

void math_compare(float a, float b, float c, out float result)
{
  result = (abs(a - b) <= max(c, 1e-5)) ? 1.0 : 0.0;
}

void math_multiply_add(float a, float b, float c, out float result)
{
  result = a * b + c;
}

/* See: https://www.iquilezles.org/www/articles/smin/smin.htm. */
void math_smoothmin(float a, float b, float c, out float result)
{
  if (c != 0.0) {
    float h = max(c - abs(a - b), 0.0) / c;
    result = min(a, b) - h * h * h * c * (1.0 / 6.0);
  }
  else {
    result = min(a, b);
  }
}

void math_smoothmax(float a, float b, float c, out float result)
{
  math_smoothmin(-a, -b, c, result);
  result = -result;
}
