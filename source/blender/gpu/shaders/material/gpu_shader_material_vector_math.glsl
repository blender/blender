/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)

void vector_math_add(vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = a + b;
}

void vector_math_subtract(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = a - b;
}

void vector_math_multiply(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = a * b;
}

void vector_math_divide(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = safe_divide(a, b);
}

void vector_math_cross(vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = cross(a, b);
}

void vector_math_project(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  float lenSquared = dot(b, b);
  outVector = (lenSquared != 0.0) ? (dot(a, b) / lenSquared) * b : vec3(0.0);
}

void vector_math_reflect(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = reflect(a, normalize(b));
}

void vector_math_dot(vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outValue = dot(a, b);
}

void vector_math_distance(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outValue = distance(a, b);
}

void vector_math_length(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outValue = length(a);
}

void vector_math_scale(vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = a * scale;
}

void vector_math_normalize(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = a;
  /* Safe version of normalize(a). */
  float lenSquared = dot(a, a);
  if (lenSquared > 0.0) {
    outVector *= inversesqrt(lenSquared);
  }
}

void vector_math_snap(vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = floor(safe_divide(a, b)) * b;
}

void vector_math_floor(vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = floor(a);
}

void vector_math_ceil(vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = ceil(a);
}

void vector_math_modulo(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = compatible_fmod(a, b);
}

void vector_math_wrap(vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = wrap(a, b, c);
}

void vector_math_fraction(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = fract(a);
}

void vector_math_absolute(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = abs(a);
}

void vector_math_minimum(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = min(a, b);
}

void vector_math_maximum(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = max(a, b);
}

void vector_math_sine(vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = sin(a);
}

void vector_math_cosine(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = cos(a);
}

void vector_math_tangent(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = tan(a);
}

void vector_math_refract(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = refract(a, normalize(b), scale);
}

void vector_math_faceforward(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = faceforward(a, b, c);
}

void vector_math_multiply_add(
    vec3 a, vec3 b, vec3 c, float scale, out vec3 outVector, out float outValue)
{
  outVector = a * b + c;
}
