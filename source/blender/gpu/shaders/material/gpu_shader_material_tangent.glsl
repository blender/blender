/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_transform_utils.glsl"

void tangent_orco_x(float3 orco_in, out float3 orco_out)
{
  orco_out = orco_in.xzy * float3(0.0f, -0.5f, 0.5f) + float3(0.0f, 0.25f, -0.25f);
}

void tangent_orco_y(float3 orco_in, out float3 orco_out)
{
  orco_out = orco_in.zyx * float3(-0.5f, 0.0f, 0.5f) + float3(0.25f, 0.0f, -0.25f);
}

void tangent_orco_z(float3 orco_in, out float3 orco_out)
{
  orco_out = orco_in.yxz * float3(-0.5f, 0.5f, 0.0f) + float3(0.25f, -0.25f, 0.0f);
}

void node_tangentmap(float4 attr_tangent, out float3 tangent)
{
  tangent = normalize(attr_tangent.xyz);
}

void node_tangent(float3 orco, out float3 T)
{
  direction_transform_object_to_world(orco, T);
  T = cross(g_data.N, normalize(cross(T, g_data.N)));
}
