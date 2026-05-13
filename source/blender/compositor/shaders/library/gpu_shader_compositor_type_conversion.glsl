/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_matrix_construct_lib.glsl"
#include "gpu_shader_math_rotation_conversion_lib.glsl"

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

int3 float_to_int3(float value)
{
  return int3(float_to_int(value));
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

float4 float_to_quaternion(float value)
{
  return to_quaternion(EulerXYZ::from_float3(float3(value))).as_float4();
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

int3 int_to_int3(int value)
{
  return int3(value);
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

int3 float2_to_int3(float2 value)
{
  return int3(int2(value), 0);
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

float4 float2_to_quaternion(float2 value)
{
  return to_quaternion(EulerXYZ::from_float3(float3(value, 0.0f))).as_float4();
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

int3 float3_to_int3(float3 value)
{
  return int3(value);
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

float4 float3_to_quaternion(float3 value)
{
  return to_quaternion(EulerXYZ::from_float3(value)).as_float4();
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

int3 color_to_int3(float4 value)
{
  return int3(value.rgb);
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

int3 float4_to_int3(float4 value)
{
  return int3(value.xyz);
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

float4 float4_to_quaternion(float4 value)
{
  return value;
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

int3 int2_to_int3(int2 value)
{
  return int3(value, 0);
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
 * Int3 to other.
 */

float int3_to_float(int3 value)
{
  return float3_to_float(float3(value));
}

int int3_to_int(int3 value)
{
  return int(int3_to_float(value));
}

int2 int3_to_int2(int3 value)
{
  return value.xy;
}

float2 int3_to_float2(int3 value)
{
  return float2(value.xy);
}

float3 int3_to_float3(int3 value)
{
  return float3(value);
}

float4 int3_to_color(int3 value)
{
  return float4(float3(value), 1.0f);
}

float4 int3_to_float4(int3 value)
{
  return float4(float3(value), 0.0f);
}

bool int3_to_bool(int3 value)
{
  return !all(equal(value, int3(0)));
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

int3 bool_to_int3(bool value)
{
  return int3(value);
}

/* --------------------------------------------------------------------
 * Float4x4 to other.
 */

float4 float4x4_to_quaternion(float4x4 mat)
{
  float3x3 mat_3x3 = normalize(to_float3x3(mat));
  return to_quaternion(to_euler(mat_3x3)).as_float4();
}

/* --------------------------------------------------------------------
 * Quaternion to other.
 */

float2 quaternion_to_float2(float4 value)
{
  Quaternion quat = Quaternion{UNPACK4(value)};
  return to_euler(from_rotation(quat)).as_float3().xy();
}

float3 quaternion_to_float3(float4 value)
{
  Quaternion quat = Quaternion{UNPACK4(value)};
  return to_euler(from_rotation(quat)).as_float3();
}

float4 quaternion_to_float4(float4 value)
{
  return value;
}

float4x4 quaternion_to_float4x4(float4 value)
{
  Quaternion quat = Quaternion{UNPACK4(value)};
  float3x3 mat_3x3 = from_rotation(quat);
  return float4x4(float4(mat_3x3[0], 0.0f),
                  float4(mat_3x3[1], 0.0f),
                  float4(mat_3x3[2], 0.0f),
                  float4(0.0f, 0.0f, 0.0f, 1.0f));
}
