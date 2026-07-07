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

int2 float_to_int2(float value)
{
  return int2(float_to_int(value));
}

float2 float_to_float2(float value)
{
  return float2(value);
}

float3 float_to_float3(float value)
{
  return float3(value);
}

float4 float_to_color(float value)
{
  return float4(float3(value), 1.0f);
}

float4 float_to_float4(float value)
{
  return float4(value);
}

bool float_to_bool(float value)
{
  return value > 0.0f;
}

/* --------------------------------------------------------------------
 * Int to other.
 */

float int_to_float(int value)
{
  return float(value);
}

int2 int_to_int2(int value)
{
  return int2(value);
}

float2 int_to_float2(int value)
{
  return float_to_float2(int_to_float(value));
}

float3 int_to_float3(int value)
{
  return float_to_float3(int_to_float(value));
}

float4 int_to_color(int value)
{
  return float_to_color(int_to_float(value));
}

float4 int_to_float4(int value)
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

float float2_to_float(float2 value)
{
  return dot(value, float2(1.0f)) / 2.0f;
}

int float2_to_int(float2 value)
{
  return float_to_int(float2_to_float(value));
}

int2 float2_to_int2(float2 value)
{
  return int2(value);
}

float3 float2_to_float3(float2 value)
{
  return float3(value, 0.0f);
}

float4 float2_to_color(float2 value)
{
  return float4(value, 0.0f, 1.0f);
}

float4 float2_to_float4(float2 value)
{
  return float4(value, 0.0f, 0.0f);
}

bool float2_to_bool(float2 value)
{
  return !all(equal(value, float2(0.0f)));
}

/* --------------------------------------------------------------------
 * Float3 to other.
 */

float float3_to_float(float3 value)
{
  return dot(value, float3(1.0f)) / 3.0f;
}

int float3_to_int(float3 value)
{
  return float_to_int(float3_to_float(value));
}

int2 float3_to_int2(float3 value)
{
  return float_to_int2(float3_to_float(value));
}

float2 float3_to_float2(float3 value)
{
  return value.xy;
}

float4 float3_to_color(float3 value)
{
  return float4(value, 1.0f);
}

float4 float3_to_float4(float3 value)
{
  return float4(value, 0.0f);
}

bool float3_to_bool(float3 value)
{
  return !all(equal(value, float3(0.0f)));
}

/* --------------------------------------------------------------------
 * Color to other.
 */

float color_to_float(float4 value, float3 luminance_coefficients)
{
  return dot(value.rgb, luminance_coefficients);
}

int color_to_int(float4 value, float3 luminance_coefficients)
{
  return float_to_int(color_to_float(value, luminance_coefficients));
}

int2 color_to_int2(float4 value)
{
  return int2(value.rg);
}

float2 color_to_float2(float4 value)
{
  return value.rg;
}

float3 color_to_float3(float4 value)
{
  return value.rgb;
}

float4 color_to_float4(float4 value)
{
  return value;
}

bool color_to_bool(float4 value, float3 luminance_coefficients)
{
  return color_to_float(value, luminance_coefficients) > 0.0f;
}

/* --------------------------------------------------------------------
 * Float4 to other.
 */

float float4_to_float(float4 value)
{
  return dot(value, float4(1.0f)) / 4.0f;
}

int float4_to_int(float4 value)
{
  return float_to_int(float4_to_float(value));
}

int2 float4_to_int2(float4 value)
{
  return int2(value.xy);
}

float2 float4_to_float2(float4 value)
{
  return value.xy;
}

float3 float4_to_float3(float4 value)
{
  return value.xyz;
}

float4 float4_to_color(float4 value)
{
  return value;
}

bool float4_to_bool(float4 value)
{
  return !all(equal(value, float4(0.0f)));
}

/* --------------------------------------------------------------------
 * Int2 to other.
 */

float int2_to_float(int2 value)
{
  return float2_to_float(float2(value));
}

int int2_to_int(int2 value)
{
  return int(int2_to_float(value));
}

float2 int2_to_float2(int2 value)
{
  return float2(value);
}

float3 int2_to_float3(int2 value)
{
  return float3(float2(value), 0.0f);
}

float4 int2_to_color(int2 value)
{
  return float4(float2(value), 0.0f, 1.0f);
}

float4 int2_to_float4(int2 value)
{
  return float4(float2(value), 0.0f, 0.0f);
}

bool int2_to_bool(int2 value)
{
  return !all(equal(value, int2(0)));
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

float2 bool_to_float2(bool value)
{
  return float2(value);
}

float3 bool_to_float3(bool value)
{
  return float3(value);
}

float4 bool_to_color(bool value)
{
  return float4(value);
}

float4 bool_to_float4(bool value)
{
  return float4(value);
}

int2 bool_to_int2(bool value)
{
  return int2(value);
}
