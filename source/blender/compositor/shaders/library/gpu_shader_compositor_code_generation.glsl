/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The file contains the implementation of functions inserted by the GPUMatrial code generator,
 * this mainly contains the [type]_from_[type] implicit conversion functions. */

float float_from_vec4(float4 value, float3 luminance_coefficients)
{
  return dot(value.xyz(), luminance_coefficients);
}

float float_from_vec3(float3 value)
{
  return dot(value, float3(1.0f)) / 3.0f;
}

float float_from_vec2(float2 value)
{
  return dot(value, float2(1.0f)) / 2.0f;
}

float2 vec2_from_vec4(float4 value)
{
  return value.xy;
}

float2 vec2_from_vec3(float3 value)
{
  return value.xy;
}

float2 vec2_from_float(float value)
{
  return float2(value);
}

float3 vec3_from_vec4(float4 value)
{
  return value.xyz;
}

float3 vec3_from_vec2(float2 value)
{
  return float3(value, 0.0f);
}

float3 vec3_from_float(float value)
{
  return float3(value);
}

float4 vec4_from_vec2(float2 value)
{
  return float4(value, 0.0f, 1.0f);
}

float4 vec4_from_vec3(float3 value)
{
  return float4(value, 1.0f);
}

float4 vec4_from_float(float value)
{
  return float4(float3(value), 1.0f);
}
