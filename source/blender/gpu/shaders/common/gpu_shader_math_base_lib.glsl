/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_glsl_cpp_stubs.hh"

/* WORKAROUND: to guard against double include in EEVEE. */
#ifndef GPU_SHADER_MATH_BASE_LIB_GLSL
#  define GPU_SHADER_MATH_BASE_LIB_GLSL

#  define M_PI 3.14159265358979323846f      /* pi */
#  define M_TAU 6.28318530717958647692f     /* tau = 2*pi */
#  define M_PI_2 1.57079632679489661923f    /* pi/2 */
#  define M_PI_4 0.78539816339744830962f    /* pi/4 */
#  define M_SQRT2 1.41421356237309504880f   /* sqrt(2) */
#  define M_SQRT1_2 0.70710678118654752440f /* 1/sqrt(2) */
#  define M_SQRT3 1.73205080756887729352f   /* sqrt(3) */
#  define M_SQRT1_3 0.57735026918962576450f /* 1/sqrt(3) */
#  define M_1_PI 0.318309886183790671538f   /* 1/pi */
#  define M_E 2.7182818284590452354f        /* e */
#  define M_LOG2E 1.4426950408889634074f    /* log_2 e */
#  define M_LOG10E 0.43429448190325182765f  /* log_10 e */
#  define M_LN2 0.69314718055994530942f     /* log_e 2 */
#  define M_LN10 2.30258509299404568402f    /* log_e 10 */

/* `powf` is really slow for raising to integer powers. */

float pow2f(float x)
{
  return x * x;
}
float pow3f(float x)
{
  return x * x * x;
}
float pow4f(float x)
{
  return pow2f(pow2f(x));
}
float pow5f(float x)
{
  return pow4f(x) * x;
}
float pow6f(float x)
{
  return pow2f(pow3f(x));
}
float pow7f(float x)
{
  return pow6f(x) * x;
}
float pow8f(float x)
{
  return pow2f(pow4f(x));
}

int square_i(int v)
{
  return v * v;
}
uint square_uint(uint v)
{
  return v * v;
}
float square(float v)
{
  return v * v;
}
float2 square(float2 v)
{
  return v * v;
}
float3 square(float3 v)
{
  return v * v;
}
float4 square(float4 v)
{
  return v * v;
}

int cube_i(int v)
{
  return v * v * v;
}
uint cube_uint(uint v)
{
  return v * v * v;
}
float cube_f(float v)
{
  return v * v * v;
}

float hypot(float x, float y)
{
  return sqrt(x * x + y * y);
}

/* Declared as _atan2 to prevent errors with `WITH_GPU_SHADER_CPP_COMPILATION` on VS2019 due
 * to `corecrt_math` conflicting functions. */

float _atan2(float y, float x)
{
  return atan(y, x);
}
#  define atan2 _atan2

/**
 * Safe `a` modulo `b`.
 * If `b` equal 0 the result will be 0.
 */
float safe_mod(float a, float b)
{
  return (b != 0.0f) ? mod(a, b) : 0.0f;
}

/**
 * A version of mod that behaves similar to C++ `std::modf`, and is safe such that it returns 0
 * when b is also 0.
 */
float compatible_mod(float a, float b)
{
  if (b != 0.0f) {
    int N = int(a / b);
    return a - N * b;
  }
  return 0.0f;
}

/**
 * Returns \a a if it is a multiple of \a b or the next multiple or \a b after \b a .
 * In other words, it is equivalent to `divide_ceil(a, b) * b`.
 * It is undefined if \a a is negative or \b b is not strictly positive.
 */
int ceil_to_multiple(int a, int b)
{
  return ((a + b - 1) / b) * b;
}
uint ceil_to_multiple(uint a, uint b)
{
  return ((a + b - 1u) / b) * b;
}

/**
 * Integer division that returns the ceiling, instead of flooring like normal C division.
 * It is undefined if \a a is negative or \b b is not strictly positive.
 */
int divide_ceil(int a, int b)
{
  return (a + b - 1) / b;
}
uint divide_ceil(uint a, uint b)
{
  return (a + b - 1u) / b;
}

/**
 * Component wise, use vector to replace min if it is smaller and max if bigger.
 */
void min_max(float value, inout float min_v, inout float max_v)
{
  min_v = min(value, min_v);
  max_v = max(value, max_v);
}

/**
 * Safe divide `a` by `b`.
 * If `b` equal 0 the result will be 0.
 */
float safe_divide(float a, float b)
{
  return (b != 0.0f) ? (a / b) : 0.0f;
}

/**
 * Safe reciprocal function. Returns `1/a`.
 * If `a` equal 0 the result will be 0.
 */
float safe_rcp(float a)
{
  return (a != 0.0f) ? (1.0f / a) : 0.0f;
}

/**
 * Safe square root function. Returns `sqrt(a)`.
 * If `a` is less or equal to 0 then the result will be 0.
 */
float safe_sqrt(float a)
{
  return sqrt(max(0.0f, a));
}

/**
 * Safe `arccosine` function. Returns `acos(a)`.
 * If `a` is greater than 1, returns 0.
 * If `a` is less than -1, returns PI.
 */
float safe_acos(float a)
{
  if (a <= -1.0f) {
    return M_PI;
  }
  else if (a >= 1.0f) {
    return 0.0f;
  }
  return acos(a);
}

/**
 * Return true if the difference between`a` and `b` is below the `epsilon` value.
 */
bool is_equal(float a, float b, const float epsilon)
{
  return abs(a - b) <= epsilon;
}

float sin_from_cos(float c)
{
  return safe_sqrt(1.0f - square(c));
}

float cos_from_sin(float s)
{
  return safe_sqrt(1.0f - square(s));
}

/**
 * A version of pow that returns a fallback value if the computation is undefined. From the spec:
 * The result is undefined if x < 0 or if x = 0 and y is less than or equal 0.
 */
float fallback_pow(float x, float y, float fallback)
{
  if (x < 0.0f || (x == 0.0f && y <= 0.0f)) {
    return fallback;
  }

  return pow(x, y);
}

/**
 * A version of pow that behaves similar to C++ std::pow.
 */
float compatible_pow(float x, float y)
{
  if (y == 0.0f) { /* x^0 -> 1, including 0^0 */
    return 1.0f;
  }

  /* GLSL pow doesn't accept negative x. */
  if (x < 0.0f) {
    if (mod(-y, 2.0f) == 0.0f) {
      return pow(-x, y);
    }
    else {
      return -pow(-x, y);
    }
  }
  else if (x == 0.0f) {
    return 0.0f;
  }

  return pow(x, y);
}

/**
 * Wrap the given value a to fall within the range [b, c].
 */
float wrap(float a, float b, float c)
{
  float range = b - c;
  /* Avoid discrepancy on some hardware due to floating point accuracy and fast math. */
  float s = (a != b) ? floor((a - c) / range) : 1.0f;
  return (range != 0.0f) ? a - range * s : c;
}

/** \} */

#endif /* GPU_SHADER_MATH_BASE_LIB_GLSL */
