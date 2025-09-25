/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

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
#define atan2 _atan2

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
 * Return true if the difference between`a` and `b` is below the `epsilon` value.
 */
bool is_equal(float a, float b, const float epsilon)
{
  return abs(a - b) <= epsilon;
}

float sin_from_cos(float c)
{
  return sqrt(max(0.0, 1.0f - square(c)));
}

float cos_from_sin(float s)
{
  return sqrt(max(0.0, 1.0f - square(s)));
}
