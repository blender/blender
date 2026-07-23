/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The file contains the implementation of functions inserted by the GPUMatrial code generator,
 * this mainly contains the [type]_to_[type] and [type]_from_[type] implicit conversion functions,
 * the former are inserted by the compositor and the latter are inserted by the code generator.
 *
 * Note that the [type]_to_[type] functions are the same as the type conversion functions but they
 * use references for the output, and have the node attribute. */

#include "gpu_shader_compositor_type_conversion.glsl"

/* --------------------------------------------------------------------
 * Float to other.
 */

[[node]]
void float_to_int(float value, int &output_value)
{
  output_value = float_to_int(value);
}

[[node]]
void float_to_int2(float value, int2 &output_value)
{
  output_value = float_to_int2(value);
}

[[node]]
void float_to_int3(float value, int3 &output_value)
{
  output_value = float_to_int3(value);
}

[[node]]
void float_to_float2(float value, float2 &output_value)
{
  output_value = float_to_float2(value);
}

[[node]]
void float_to_float3(float value, float3 &output_value)
{
  output_value = float_to_float3(value);
}

[[node]]
void float_to_color(float value, float4 &output_value)
{
  output_value = float_to_color(value);
}

[[node]]
void float_to_float4(float value, float4 &output_value)
{
  output_value = float_to_float4(value);
}

[[node]]
void float_to_bool(float value, bool &output_value)
{
  output_value = float_to_bool(value);
}

[[node]]
void float_to_quaternion(float value, float4 &output_value)
{
  output_value = float_to_quaternion(value);
}

/* --------------------------------------------------------------------
 * Int to other.
 */

[[node]]
void int_to_float(int value, float &output_value)
{
  output_value = int_to_float(value);
}

[[node]]
void int_to_int2(int value, int2 &output_value)
{
  output_value = int_to_int2(value);
}

[[node]]
void int_to_int3(int value, int3 &output_value)
{
  output_value = int_to_int3(value);
}

[[node]]
void int_to_float2(int value, float2 &output_value)
{
  output_value = int_to_float2(value);
}

[[node]]
void int_to_float3(int value, float3 &output_value)
{
  output_value = int_to_float3(value);
}

[[node]]
void int_to_color(int value, float4 &output_value)
{
  output_value = int_to_color(value);
}

[[node]]
void int_to_float4(int value, float4 &output_value)
{
  output_value = int_to_float4(value);
}

[[node]]
void int_to_bool(int value, bool &output_value)
{
  output_value = int_to_bool(value);
}

/* --------------------------------------------------------------------
 * Float2 to other.
 */

[[node]]
void float2_to_float(float2 value, float &output_value)
{
  output_value = float2_to_float(value);
}

[[node]]
void float2_to_int(float2 value, int &output_value)
{
  output_value = float2_to_int(value);
}

[[node]]
void float2_to_int2(float2 value, int2 &output_value)
{
  output_value = float2_to_int2(value);
}

[[node]]
void float2_to_int3(float2 value, int3 &output_value)
{
  output_value = float2_to_int3(value);
}

[[node]]
void float2_to_float3(float2 value, float3 &output_value)
{
  output_value = float2_to_float3(value);
}

[[node]]
void float2_to_color(float2 value, float4 &output_value)
{
  output_value = float2_to_color(value);
}

[[node]]
void float2_to_float4(float2 value, float4 &output_value)
{
  output_value = float2_to_float4(value);
}

[[node]]
void float2_to_bool(float2 value, bool &output_value)
{
  output_value = float2_to_bool(value);
}

[[node]]
void float2_to_quaternion(float2 value, float4 &output_value)
{
  output_value = float2_to_quaternion(value);
}

/* --------------------------------------------------------------------
 * Float3 to other.
 */

[[node]]
void float3_to_float(float3 value, float &output_value)
{
  output_value = float3_to_float(value);
}

[[node]]
void float3_to_int(float3 value, int &output_value)
{
  output_value = float3_to_int(value);
}

[[node]]
void float3_to_int2(float3 value, int2 &output_value)
{
  output_value = float3_to_int2(value);
}

[[node]]
void float3_to_int3(float3 value, int3 &output_value)
{
  output_value = float3_to_int3(value);
}

[[node]]
void float3_to_float2(float3 value, float2 &output_value)
{
  output_value = float3_to_float2(value);
}

[[node]]
void float3_to_color(float3 value, float4 &output_value)
{
  output_value = float3_to_color(value);
}

[[node]]
void float3_to_float4(float3 value, float4 &output_value)
{
  output_value = float3_to_float4(value);
}

[[node]]
void float3_to_bool(float3 value, bool &output_value)
{
  output_value = float3_to_bool(value);
}

[[node]]
void float3_to_quaternion(float3 value, float4 &output_value)
{
  output_value = float3_to_quaternion(value);
}

/* --------------------------------------------------------------------
 * Color to other.
 */

[[node]]
void color_to_float(float4 value, float3 luminance_coefficients, float &output_value)
{
  output_value = color_to_float(value, luminance_coefficients);
}

[[node]]
void color_to_int(float4 value, float3 luminance_coefficients, int &output_value)
{
  output_value = color_to_int(value, luminance_coefficients);
}

[[node]]
void color_to_int2(float4 value, int2 &output_value)
{
  output_value = color_to_int2(value);
}

[[node]]
void color_to_int3(float4 value, int3 &output_value)
{
  output_value = color_to_int3(value);
}

[[node]]
void color_to_float2(float4 value, float2 &output_value)
{
  output_value = color_to_float2(value);
}

[[node]]
void color_to_float3(float4 value, float3 &output_value)
{
  output_value = color_to_float3(value);
}

[[node]]
void color_to_float4(float4 value, float4 &output_value)
{
  output_value = color_to_float4(value);
}

[[node]]
void color_to_bool(float4 value, float3 luminance_coefficients, bool &output_value)
{
  output_value = color_to_bool(value, luminance_coefficients);
}

/* --------------------------------------------------------------------
 * Float4 to other.
 */

[[node]]
void float4_to_float(float4 value, float &output_value)
{
  output_value = float4_to_float(value);
}

[[node]]
void float4_to_int(float4 value, int &output_value)
{
  output_value = float4_to_int(value);
}

[[node]]
void float4_to_int2(float4 value, int2 &output_value)
{
  output_value = float4_to_int2(value);
}

[[node]]
void float4_to_int3(float4 value, int3 &output_value)
{
  output_value = float4_to_int3(value);
}

[[node]]
void float4_to_float2(float4 value, float2 &output_value)
{
  output_value = float4_to_float2(value);
}

[[node]]
void float4_to_float3(float4 value, float3 &output_value)
{
  output_value = float4_to_float3(value);
}

[[node]]
void float4_to_color(float4 value, float4 &output_value)
{
  output_value = float4_to_color(value);
}

[[node]]
void float4_to_bool(float4 value, bool &output_value)
{
  output_value = float4_to_bool(value);
}

[[node]]
void float4_to_quaternion(float4 value, float4 &output_value)
{
  output_value = float4_to_quaternion(value);
}

/* --------------------------------------------------------------------
 * Int2 to other.
 */

[[node]]
void int2_to_float(int2 value, float &output_value)
{
  output_value = int2_to_float(value);
}

[[node]]
void int2_to_int(int2 value, int &output_value)
{
  output_value = int2_to_int(value);
}

[[node]]
void int2_to_int3(int2 value, int3 &output_value)
{
  output_value = int2_to_int3(value);
}

[[node]]
void int2_to_float2(int2 value, float2 &output_value)
{
  output_value = int2_to_float2(value);
}

[[node]]
void int2_to_float3(int2 value, float3 &output_value)
{
  output_value = int2_to_float3(value);
}

[[node]]
void int2_to_color(int2 value, float4 &output_value)
{
  output_value = int2_to_color(value);
}

[[node]]
void int2_to_float4(int2 value, float4 &output_value)
{
  output_value = int2_to_float4(value);
}

[[node]]
void int2_to_bool(int2 value, bool &output_value)
{
  output_value = int2_to_bool(value);
}

/* --------------------------------------------------------------------
 * Int3 to other.
 */

[[node]]
void int3_to_float(int3 value, float &output_value)
{
  output_value = int3_to_float(value);
}

[[node]]
void int3_to_int(int3 value, int &output_value)
{
  output_value = int3_to_int(value);
}

[[node]]
void int3_to_int2(int3 value, int2 &output_value)
{
  output_value = int3_to_int2(value);
}

[[node]]
void int3_to_float2(int3 value, float2 &output_value)
{
  output_value = int3_to_float2(value);
}

[[node]]
void int3_to_float3(int3 value, float3 &output_value)
{
  output_value = int3_to_float3(value);
}

[[node]]
void int3_to_color(int3 value, float4 &output_value)
{
  output_value = int3_to_color(value);
}

[[node]]
void int3_to_float4(int3 value, float4 &output_value)
{
  output_value = int3_to_float4(value);
}

[[node]]
void int3_to_bool(int3 value, bool &output_value)
{
  output_value = int3_to_bool(value);
}

/* --------------------------------------------------------------------
 * Bool to other.
 */

[[node]]
void bool_to_float(bool value, float &output_value)
{
  output_value = bool_to_float(value);
}

[[node]]
void bool_to_int(bool value, int &output_value)
{
  output_value = bool_to_int(value);
}

[[node]]
void bool_to_float2(bool value, float2 &output_value)
{
  output_value = bool_to_float2(value);
}

[[node]]
void bool_to_float3(bool value, float3 &output_value)
{
  output_value = bool_to_float3(value);
}

[[node]]
void bool_to_color(bool value, float4 &output_value)
{
  output_value = bool_to_color(value);
}

[[node]]
void bool_to_float4(bool value, float4 &output_value)
{
  output_value = bool_to_float4(value);
}

[[node]]
void bool_to_int2(bool value, int2 &output_value)
{
  output_value = bool_to_int2(value);
}

[[node]]
void bool_to_int3(bool value, int3 &output_value)
{
  output_value = bool_to_int3(value);
}

/* --------------------------------------------------------------------
 * Float4x4 to other.
 */

[[node]]
void float4x4_to_quaternion(float4x4 mat, float4 &output_value)
{
  output_value = float4x4_to_quaternion(mat);
}

/* --------------------------------------------------------------------
 * Quaternion to other.
 */

[[node]]
void quaternion_to_float2(float4 value, float2 &output_value)
{
  output_value = quaternion_to_float2(value);
}

[[node]]
void quaternion_to_float3(float4 value, float3 &output_value)
{
  output_value = quaternion_to_float3(value);
}

[[node]]
void quaternion_to_float4(float4 value, float4 &output_value)
{
  output_value = quaternion_to_float4(value);
}

[[node]]
void quaternion_to_float4x4(float4 value, float4x4 &output_value)
{
  output_value = quaternion_to_float4x4(value);
}

/* --------------------------------------------------------------------
 * Internal Code Generation Functions.
 */

float float_from_float4(float4 value, float3 luminance_coefficients)
{
  return color_to_float(value, luminance_coefficients);
}

float float_from_float3(float3 value)
{
  return float3_to_float(value);
}

float float_from_float2(float2 value)
{
  return float2_to_float(value);
}

float float_from_int3(int3 value)
{
  return int3_to_float(value);
}

float float_from_int2(int2 value)
{
  return int2_to_float(value);
}

float float_from_int(int value)
{
  return int_to_float(value);
}

float float_from_bool(bool value)
{
  return bool_to_float(value);
}

float2 float2_from_float4(float4 value)
{
  return color_to_float2(value);
}

float2 float2_from_float3(float3 value)
{
  return float3_to_float2(value);
}

float2 float2_from_float(float value)
{
  return float_to_float2(value);
}

float2 float2_from_int3(int3 value)
{
  return int3_to_float2(value);
}

float2 float2_from_int2(int2 value)
{
  return int2_to_float2(value);
}

float2 float2_from_int(int value)
{
  return int_to_float2(value);
}

float2 float2_from_bool(bool value)
{
  return bool_to_float2(value);
}

float3 float3_from_float4(float4 value)
{
  return color_to_float3(value);
}

float3 float3_from_float2(float2 value)
{
  return float2_to_float3(value);
}

float3 float3_from_float(float value)
{
  return float_to_float3(value);
}

float3 float3_from_int3(int3 value)
{
  return int3_to_float3(value);
}

float3 float3_from_int2(int2 value)
{
  return int2_to_float3(value);
}

float3 float3_from_int(int value)
{
  return int_to_float3(value);
}

float3 float3_from_bool(bool value)
{
  return bool_to_float3(value);
}

float4 float4_from_float3(float3 value)
{
  return float3_to_color(value);
}

float4 float4_from_float2(float2 value)
{
  return float2_to_color(value);
}

float4 float4_from_float(float value)
{
  return float_to_color(value);
}

float4 float4_from_int3(int3 value)
{
  return int3_to_color(value);
}

float4 float4_from_int2(int2 value)
{
  return int2_to_color(value);
}

float4 float4_from_int(int value)
{
  return int_to_color(value);
}

float4 float4_from_bool(bool value)
{
  return bool_to_color(value);
}

float4 float4_from_float4x4(float4x4 mat)
{
  return float4x4_to_quaternion(mat);
}

float4x4 float4x4_from_float4(float4 value)
{
  return quaternion_to_float4x4(value);
}

int int_from_float4(float4 value)
{
  return float4_to_int(value);
}

int int_from_float3(float3 value)
{
  return float3_to_int(value);
}

int int_from_float2(float2 value)
{
  return float2_to_int(value);
}

int int_from_float(float value)
{
  return float_to_int(value);
}

int int_from_int3(int3 value)
{
  return int3_to_int(value);
}

int int_from_int2(int2 value)
{
  return int2_to_int(value);
}

int int_from_bool(bool value)
{
  return bool_to_int(value);
}

int2 int2_from_float4(float4 value)
{
  return float4_to_int2(value);
}

int2 int2_from_float3(float3 value)
{
  return float3_to_int2(value);
}

int2 int2_from_float2(float2 value)
{
  return float2_to_int2(value);
}

int2 int2_from_float(float value)
{
  return float_to_int2(value);
}

int2 int2_from_int3(int3 value)
{
  return int3_to_int2(value);
}

int2 int2_from_int(int value)
{
  return int_to_int2(value);
}

int2 int2_from_bool(bool value)
{
  return bool_to_int2(value);
}

int3 int3_from_float4(float4 value)
{
  return float4_to_int3(value);
}

int3 int3_from_float3(float3 value)
{
  return float3_to_int3(value);
}

int3 int3_from_float2(float2 value)
{
  return float2_to_int3(value);
}

int3 int3_from_float(float value)
{
  return float_to_int3(value);
}

int3 int3_from_int2(int2 value)
{
  return int2_to_int3(value);
}

int3 int3_from_int(int value)
{
  return int_to_int3(value);
}

int3 int3_from_bool(bool value)
{
  return bool_to_int3(value);
}

bool bool_from_float4(float4 value)
{
  return float4_to_bool(value);
}

bool bool_from_float3(float3 value)
{
  return float3_to_bool(value);
}

bool bool_from_float2(float2 value)
{
  return float2_to_bool(value);
}

bool bool_from_float(float value)
{
  return float_to_bool(value);
}

bool bool_from_int3(int3 value)
{
  return int3_to_bool(value);
}

bool bool_from_int2(int2 value)
{
  return int2_to_bool(value);
}

bool bool_from_int(int value)
{
  return int_to_bool(value);
}
