/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_safe_lib.glsl"

float3 vector_math_safe_normalize(float3 a)
{
  /* Match the safe normalize function in Cycles by defaulting to float3(0.0f) */
  float length_sqr = dot(a, a);
  return (length_sqr > 1e-35f) ? a * inversesqrt(length_sqr) : float3(0.0f);
}

[[node]]
void vector_math_add(float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = a + b;
}

[[node]]
void vector_math_subtract(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = a - b;
}

[[node]]
void vector_math_multiply(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = a * b;
}

[[node]]
void vector_math_divide(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = safe_divide(a, b);
}

[[node]]
void vector_math_cross(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = cross(a, b);
}

[[node]]
void vector_math_project(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  float lenSquared = dot(b, b);
  outVector = (lenSquared != 0.0f) ? (dot(a, b) / lenSquared) * b : float3(0.0f);
}

[[node]]
void vector_math_reflect(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = reflect(a, vector_math_safe_normalize(b));
}

[[node]]
void vector_math_dot(float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outValue = dot(a, b);
}

[[node]]
void vector_math_distance(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outValue = distance(a, b);
}

[[node]]
void vector_math_length(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outValue = length(a);
}

[[node]]
void vector_math_scale(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = a * scale;
}

[[node]]
void vector_math_normalize(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = a;
  /* Safe version of normalize(a). */
  float lenSquared = dot(a, a);
  if (lenSquared > 0.0f) {
    outVector *= inversesqrt(lenSquared);
  }
}

[[node]]
void vector_math_snap(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = floor(safe_divide(a, b)) * b;
}

[[node]]
void vector_math_floor(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = floor(a);
}

[[node]]
void vector_math_ceil(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = ceil(a);
}

[[node]]
void vector_math_modulo(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = compatible_mod(a, b);
}

[[node]]
void vector_math_wrap(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = wrap(a, b, c);
}

[[node]]
void vector_math_fraction(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = fract(a);
}

[[node]]
void vector_math_absolute(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = abs(a);
}

[[node]]
void vector_math_power(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = compatible_pow(a, b);
}

[[node]]
void vector_math_sign(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = sign(a);
}

[[node]]
void vector_math_minimum(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = min(a, b);
}

[[node]]
void vector_math_maximum(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = max(a, b);
}

[[node]]
void vector_math_sine(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = sin(a);
}

[[node]]
void vector_math_cosine(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = cos(a);
}

[[node]]
void vector_math_tangent(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = tan(a);
}

[[node]]
void vector_math_refract(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = refract(a, vector_math_safe_normalize(b), scale);
}

[[node]]
void vector_math_faceforward(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = faceforward(a, b, c);
}

[[node]]
void vector_math_multiply_add(
    float3 a, float3 b, float3 c, float scale, float3 &outVector, float &outValue)
{
  outVector = a * b + c;
}
