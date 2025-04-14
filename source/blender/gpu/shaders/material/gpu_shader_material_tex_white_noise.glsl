/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_hash.glsl"

/* White Noise */

void node_white_noise_1d(float3 vector, float w, out float value, out float4 color)
{
  value = hash_float_to_float(w);
  color = float4(hash_float_to_vec3(w), 1.0f);
}

void node_white_noise_2d(float3 vector, float w, out float value, out float4 color)
{
  value = hash_vec2_to_float(vector.xy);
  color = float4(hash_vec2_to_vec3(vector.xy), 1.0f);
}

void node_white_noise_3d(float3 vector, float w, out float value, out float4 color)
{
  value = hash_vec3_to_float(vector);
  color = float4(hash_vec3_to_vec3(vector), 1.0f);
}

void node_white_noise_4d(float3 vector, float w, out float value, out float4 color)
{
  value = hash_vec4_to_float(float4(vector, w));
  color = float4(hash_vec4_to_vec3(float4(vector, w)), 1.0f);
}
