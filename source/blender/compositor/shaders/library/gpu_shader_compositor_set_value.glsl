/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_matrix_construct_lib.glsl"

[[node]]
void set_float(float input_value, float &output_value)
{
  output_value = input_value;
}

[[node]]
void set_float2(float2 input_value, float2 &output_value)
{
  output_value = input_value;
}

[[node]]
void set_float3(float3 input_value, float3 &output_value)
{
  output_value = input_value;
}

[[node]]
void set_float4(float4 input_value, float4 &output_value)
{
  output_value = input_value;
}

[[node]]
void set_color(float4 input_value, float4 &output_value)
{
  output_value = input_value;
}

[[node]]
void set_float4x4(float4x4 input_value, float4x4 &output_value)
{
  output_value = input_value;
}

[[node]]
void set_float4x4_default(float4x4 &output_value)
{
  output_value = mat4x4_identity();
}
