/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* WORKAROUND: to guard against double include in EEVEE. */
#ifndef GPU_SHADER_MATH_BASE_LIB_GLSL
#define GPU_SHADER_MATH_BASE_LIB_GLSL

#define M_PI 3.14159265358979323846      /* pi */
#define M_TAU 6.28318530717958647692     /* tau = 2*pi */
#define M_PI_2 1.57079632679489661923    /* pi/2 */
#define M_PI_4 0.78539816339744830962    /* pi/4 */
#define M_SQRT2 1.41421356237309504880   /* sqrt(2) */
#define M_SQRT1_2 0.70710678118654752440 /* 1/sqrt(2) */
#define M_SQRT3 1.73205080756887729352   /* sqrt(3) */
#define M_SQRT1_3 0.57735026918962576450 /* 1/sqrt(3) */
#define M_1_PI 0.318309886183790671538   /* 1/pi */
#define M_E 2.7182818284590452354        /* e */
#define M_LOG2E 1.4426950408889634074    /* log_2 e */
#define M_LOG10E 0.43429448190325182765  /* log_10 e */
#define M_LN2 0.69314718055994530942     /* log_e 2 */
#define M_LN10 2.30258509299404568402    /* log_e 10 */

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
vec2 square(vec2 v)
{
  return v * v;
}
vec3 square(vec3 v)
{
  return v * v;
}
vec4 square(vec4 v)
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

float atan2(float y, float x)
{
  return atan(y, x);
}

/**
 * Safe `a` modulo `b`.
 * If `b` equal 0 the result will be 0.
 */
float safe_mod(float a, float b)
{
  return (b != 0.0) ? mod(a, b) : 0.0;
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
  return (b != 0.0) ? (a / b) : 0.0;
}

/**
 * Safe reciprocal function. Returns `1/a`.
 * If `a` equal 0 the result will be 0.
 */
float safe_rcp(float a)
{
  return (a != 0.0) ? (1.0 / a) : 0.0;
}

/**
 * Safe square root function. Returns `sqrt(a)`.
 * If `a` is less or equal to 0 then the result will be 0.
 */
float safe_sqrt(float a)
{
  return sqrt(max(0.0, a));
}

/**
 * Safe `arccosine` function. Returns `acos(a)`.
 * If `a` is greater than 1, returns 0.
 * If `a` is less than -1, returns PI.
 */
float safe_acos(float a)
{
  if (a <= -1.0) {
    return M_PI;
  }
  else if (a >= 1.0) {
    return 0.0;
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

/** \} */

#endif /* GPU_SHADER_MATH_BASE_LIB_GLSL */
