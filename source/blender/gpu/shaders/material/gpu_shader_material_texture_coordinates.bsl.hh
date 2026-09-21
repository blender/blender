/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_transform_utils.bsl.hh"

[[node]]
void node_tex_coord_position(const ShadingData &sd, float3 &out_pos)
{
  out_pos = sd.P;
}

[[node]]
void node_tex_coord(float4x4 obmatinv,
                    float3 attr_orco,
                    float4 attr_uv,
                    [[resource_table]] KernelGlobals &kg,
                    const ShadingData &sd,
                    float3 &generated,
                    float3 &normal,
                    float3 &uv,
                    float3 &object,
                    float3 &camera,
                    float3 &window,
                    float3 &reflection)
{
  Coordinates coords = coordinate_impl(kg, sd, sd.P, sd.N);
  generated = attr_orco;
  normal_transform_world_to_object(sd.N, kg, sd, normal);
  uv = attr_uv.xyz;
  bool valid_mat = (obmatinv[3][3] != 0.0f);
  if (valid_mat) {
    object = (obmatinv * float4(sd.P, 1.0f)).xyz;
  }
  else {
    point_transform_world_to_object(sd.P, kg, sd, object);
  }
  camera = coords.camera;
  window = coords.screen;
  reflection = coords.reflect;
}
