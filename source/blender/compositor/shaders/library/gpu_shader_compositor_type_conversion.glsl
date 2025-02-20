/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* --------------------------------------------------------------------
 * Float to other.
 */

int float_to_int(float value)
{
  return int(value);
}

vec4 float_to_vector(float value)
{
  return vec4(vec3(value), 1.0);
}

vec4 float_to_color(float value)
{
  return vec4(vec3(value), 1.0);
}

vec4 float_to_float4(float value)
{
  return vec4(value);
}

/* --------------------------------------------------------------------
 * Int to other.
 */

float int_to_float(int value)
{
  return float(value);
}

vec4 int_to_vector(int value)
{
  return float_to_vector(int_to_float(value));
}

vec4 int_to_color(int value)
{
  return float_to_color(int_to_float(value));
}

vec4 int_to_float4(int value)
{
  return float_to_float4(int_to_float(value));
}

/* --------------------------------------------------------------------
 * Vector to other.
 */

float vector_to_float(vec4 value)
{
  return dot(value.xyz, vec3(1.0)) / 3.0;
}

int vector_to_int(vec4 value)
{
  return float_to_int(vector_to_float(value));
}

vec4 vector_to_color(vec4 value)
{
  return vec4(value.xyz, 1.0);
}

vec4 vector_to_float4(vec4 value)
{
  return value;
}

/* --------------------------------------------------------------------
 * Color to other.
 */

float color_to_float(vec4 value, vec3 luminance_coefficients)
{
  return dot(value.rgb, luminance_coefficients);
}

int color_to_int(vec4 value, vec3 luminance_coefficients)
{
  return float_to_int(color_to_float(value, luminance_coefficients));
}

vec4 color_to_vector(vec4 value)
{
  return value;
}

vec4 color_to_float4(vec4 value)
{
  return value;
}

/* --------------------------------------------------------------------
 * Float4 to other.
 */

float float4_to_float(vec4 value)
{
  return dot(value, vec4(1.0)) / 4.0;
}

int float4_to_int(vec4 value)
{
  return float_to_int(float4_to_float(value));
}

vec4 float4_to_vector(vec4 value)
{
  return value;
}

vec4 float4_to_color(vec4 value)
{
  return value;
}

/* --------------------------------------------------------------------
 * GPUMatrial-specific implicit conversion functions.
 *
 * Those should have the same interface and names as the macros in gpu_shader_codegen_lib.glsl
 * since the GPUMaterial compiler inserts those hard coded names. */

float float_from_vec4(vec4 vector, vec3 luminance_coefficients)
{
  return color_to_float(vector, luminance_coefficients);
}

float float_from_vec3(vec3 vector)
{
  return dot(vector, vec3(1.0)) / 3.0;
}

vec3 vec3_from_vec4(vec4 vector)
{
  return vector.rgb;
}

vec3 vec3_from_float(float value)
{
  return vec3(value);
}

vec4 vec4_from_vec3(vec3 vector)
{
  return vec4(vector, 1.0);
}

vec4 vec4_from_float(float value)
{
  return vec4(vec3(value), 1.0);
}
