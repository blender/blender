/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_transform_utils.glsl"

void node_tex_coord_position(out float3 out_pos)
{
  out_pos = g_data.P;
}

void node_tex_coord(float4x4 obmatinv,
                    float3 attr_orco,
                    float4 attr_uv,
                    out float3 generated,
                    out float3 normal,
                    out float3 uv,
                    out float3 object,
                    out float3 camera,
                    out float3 window,
                    out float3 reflection)
{
  generated = attr_orco;
  normal_transform_world_to_object(g_data.N, normal);
  uv = attr_uv.xyz;
  bool valid_mat = (obmatinv[3][3] != 0.0f);
  if (valid_mat) {
    object = (obmatinv * float4(g_data.P, 1.0f)).xyz;
  }
  else {
    point_transform_world_to_object(g_data.P, object);
  }
  camera = coordinate_camera(g_data.P);
  window = coordinate_screen(g_data.P);
  reflection = coordinate_reflect(g_data.P, g_data.N);
}
