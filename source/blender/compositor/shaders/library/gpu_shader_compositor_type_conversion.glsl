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

ivec2 float_to_int2(float value)
{
  return ivec2(float_to_int(value));
}

vec2 float_to_float2(float value)
{
  return vec2(value);
}

vec3 float_to_float3(float value)
{
  return vec3(value);
}

vec4 float_to_color(float value)
{
  return vec4(vec3(value), 1.0);
}

vec4 float_to_float4(float value)
{
  return vec4(value);
}

bool float_to_bool(float value)
{
  return value > 0.0;
}

/* --------------------------------------------------------------------
 * Int to other.
 */

float int_to_float(int value)
{
  return float(value);
}

ivec2 int_to_int2(int value)
{
  return ivec2(value);
}

vec2 int_to_float2(int value)
{
  return float_to_float2(int_to_float(value));
}

vec3 int_to_float3(int value)
{
  return float_to_float3(int_to_float(value));
}

vec4 int_to_color(int value)
{
  return float_to_color(int_to_float(value));
}

vec4 int_to_float4(int value)
{
  return float_to_float4(int_to_float(value));
}

bool int_to_bool(int value)
{
  return value > 0;
}

/* --------------------------------------------------------------------
 * Float2 to other.
 */

float float2_to_float(vec2 value)
{
  return dot(value, vec2(1.0)) / 2.0;
}

int float2_to_int(vec2 value)
{
  return float_to_int(float2_to_float(value));
}

ivec2 float2_to_int2(vec2 value)
{
  return ivec2(value);
}

vec3 float2_to_float3(vec2 value)
{
  return vec3(value, 0.0);
}

vec4 float2_to_color(vec2 value)
{
  return vec4(value, 0.0, 1.0);
}

vec4 float2_to_float4(vec2 value)
{
  return vec4(value, 0.0, 0.0);
}

bool float2_to_bool(vec2 value)
{
  return !all(equal(value, vec2(0.0)));
}

/* --------------------------------------------------------------------
 * Float3 to other.
 */

float float3_to_float(vec3 value)
{
  return dot(value, vec3(1.0)) / 3.0;
}

int float3_to_int(vec3 value)
{
  return float_to_int(float3_to_float(value));
}

ivec2 float3_to_int2(vec3 value)
{
  return float_to_int2(float3_to_float(value));
}

vec2 float3_to_float2(vec3 value)
{
  return value.xy;
}

vec4 float3_to_color(vec3 value)
{
  return vec4(value, 1.0);
}

vec4 float3_to_float4(vec3 value)
{
  return vec4(value, 0.0);
}

bool float3_to_bool(vec3 value)
{
  return !all(equal(value, vec3(0.0)));
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

ivec2 color_to_int2(vec4 value)
{
  return ivec2(value.rg);
}

vec2 color_to_float2(vec4 value)
{
  return value.rg;
}

vec3 color_to_float3(vec4 value)
{
  return value.rgb;
}

vec4 color_to_float4(vec4 value)
{
  return value;
}

bool color_to_bool(vec4 value, vec3 luminance_coefficients)
{
  return color_to_float(value, luminance_coefficients) > 0.0;
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

ivec2 float4_to_int2(vec4 value)
{
  return ivec2(value.xy);
}

vec2 float4_to_float2(vec4 value)
{
  return value.xy;
}

vec3 float4_to_float3(vec4 value)
{
  return value.xyz;
}

vec4 float4_to_color(vec4 value)
{
  return value;
}

bool float4_to_bool(vec4 value)
{
  return !all(equal(value, vec4(0.0)));
}

/* --------------------------------------------------------------------
 * Int2 to other.
 */

float int2_to_float(ivec2 value)
{
  return float2_to_float(vec2(value));
}

int int2_to_int(ivec2 value)
{
  return int(int2_to_float(value));
}

vec2 int2_to_float2(ivec2 value)
{
  return vec2(value);
}

vec3 int2_to_float3(ivec2 value)
{
  return vec3(vec2(value), 0.0);
}

vec4 int2_to_color(ivec2 value)
{
  return vec4(vec2(value), 0.0, 1.0);
}

vec4 int2_to_float4(ivec2 value)
{
  return vec4(vec2(value), 0.0, 0.0);
}

bool int2_to_bool(ivec2 value)
{
  return !all(equal(value, ivec2(0)));
}

/* --------------------------------------------------------------------
 * Bool to other.
 */

float bool_to_float(bool value)
{
  return float(value);
}

int bool_to_int(bool value)
{
  return int(value);
}

vec2 bool_to_float2(bool value)
{
  return vec2(value);
}

vec3 bool_to_float3(bool value)
{
  return vec3(value);
}

vec4 bool_to_color(bool value)
{
  return vec4(value);
}

vec4 bool_to_float4(bool value)
{
  return vec4(value);
}

ivec2 bool_to_int2(bool value)
{
  return ivec2(value);
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
