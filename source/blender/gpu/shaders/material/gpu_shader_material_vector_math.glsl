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

void vector_math_add(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = a + b;
}

void vector_math_subtract(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = a - b;
}

void vector_math_multiply(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = a * b;
}

void vector_math_divide(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = safe_divide(a, b);
}

void vector_math_cross(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = cross(a, b);
}

void vector_math_project(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  float lenSquared = dot(b, b);
  outVector = (lenSquared != 0.0f) ? (dot(a, b) / lenSquared) * b : float3(0.0f);
}

void vector_math_reflect(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = reflect(a, vector_math_safe_normalize(b));
}

void vector_math_dot(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outValue = dot(a, b);
}

void vector_math_distance(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outValue = distance(a, b);
}

void vector_math_length(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outValue = length(a);
}

void vector_math_scale(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = a * scale;
}

void vector_math_normalize(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = a;
  /* Safe version of normalize(a). */
  float lenSquared = dot(a, a);
  if (lenSquared > 0.0f) {
    outVector *= inversesqrt(lenSquared);
  }
}

void vector_math_snap(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = floor(safe_divide(a, b)) * b;
}

void vector_math_floor(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = floor(a);
}

void vector_math_ceil(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = ceil(a);
}

void vector_math_modulo(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = compatible_mod(a, b);
}

void vector_math_wrap(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = wrap(a, b, c);
}

void vector_math_fraction(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = fract(a);
}

void vector_math_absolute(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = abs(a);
}

void vector_math_power(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = compatible_pow(a, b);
}

void vector_math_sign(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = sign(a);
}

void vector_math_minimum(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = min(a, b);
}

void vector_math_maximum(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = max(a, b);
}

void vector_math_sine(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = sin(a);
}

void vector_math_cosine(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = cos(a);
}

void vector_math_tangent(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = tan(a);
}

void vector_math_refract(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = refract(a, vector_math_safe_normalize(b), scale);
}

void vector_math_faceforward(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = faceforward(a, b, c);
}

void vector_math_multiply_add(
    float3 a, float3 b, float3 c, float scale, out float3 outVector, out float outValue)
{
  outVector = a * b + c;
}
