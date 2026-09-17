/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

[[node]]
void input_normal_displaced(const ShadingData &sd, float3 &out_N)
{
#ifdef MAT_DISPLACEMENT_BUMP
  out_N = sd.N;
#else
  out_N = sd.Ni;
#endif
}

[[node]]
void input_normal_original(const ShadingData &sd, float3 &out_N)
{
  out_N = sd.Ni;
}

[[node]]
void node_normal_map(float4 T,
                     float strength,
                     float3 tex_normal,
                     float3 N,
                     [[resource_table]] KernelGlobals &kg,
                     const ShadingData &sd,
                     float3 &out_N)
{
  if (all(equal(T, float4(0.0f, 0.0f, 0.0f, 1.0f)))) {
    out_N = N;
    return;
  }
  T *= (FrontFacing ? 1.0f : -1.0f);
  float3 B = T.w * cross(N, T.xyz);
  /* TODO(fclem): EEVEE implementation leaking. */
  B *= (kg.object_infos_get(sd).flag & OBJECT_NEGATIVE_SCALE) != 0 ? -1.0f : 1.0f;

  /* Apply strength here instead of in node_normal_map_mix for T space. */
  tex_normal.xy *= strength;
  tex_normal.z = mix(1.0f, tex_normal.z, saturate(strength));

  out_N = tex_normal.x * T.xyz + tex_normal.y * B + tex_normal.z * N;
  out_N = normalize(out_N);
}

[[node]]
void color_to_normal_new_shading(float3 color, float3 &normal)
{
  normal = float3(2.0f) * color - float3(1.0f);
}

[[node]]
void color_to_blender_normal_new_shading(float3 color, float3 &normal)
{
  normal = float3(2.0f, -2.0f, -2.0f) * color - float3(1.0f);
}

[[node]]
void color_invert_green_channel(float3 color, float3 &result)
{
  result = float3(color.x, -color.y, color.z);
}

[[node]]
void node_normal_map_mix(float strength, float3 new_normal, const ShadingData &sd, float3 &out_N)
{
  out_N = normalize(mix(sd.N, new_normal, max(0.0f, strength)));
}
