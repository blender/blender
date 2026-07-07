/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

#include "gpu_shader_math_constants_lib.glsl"

/**
 * Safe `a` modulo `b`.
 * If `b` equal 0 the result will be 0.
 */
float safe_mod(float a, float b)
{
  return (b != 0.0f) ? mod(a, b) : 0.0f;
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
