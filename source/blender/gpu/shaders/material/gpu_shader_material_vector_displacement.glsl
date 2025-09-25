/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_transform_utils.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

void node_vector_displacement_tangent(
    float4 vector, float midlevel, float scale, float4 T, out float3 result)
{
  float3 oN, oT, oB;
  normal_transform_world_to_object(g_data.N, oN);
  normal_transform_world_to_object(T.xyz, oT);
  oN = normalize(oN);
  oT = normalize(oT);
  oB = T.w * safe_normalize(cross(oN, oT));

  float3 disp = (vector.xyz - midlevel) * scale;
  disp = disp.x * oT + disp.y * oN + disp.z * oB;
  direction_transform_object_to_world(disp, result);
}

void node_vector_displacement_object(float4 vector, float midlevel, float scale, out float3 result)
{
  float3 disp = (vector.xyz - midlevel) * scale;
  direction_transform_object_to_world(disp, result);
}

void node_vector_displacement_world(float4 vector, float midlevel, float scale, out float3 result)
{
  result = (vector.xyz - midlevel) * scale;
}
