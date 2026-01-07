/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_material_tangent.glsl"

[[node]]
void node_geometry(float3 orco_attr,
                   float3 &position,
                   float3 &normal,
                   float3 &tangent,
                   float3 &true_normal,
                   float3 &incoming,
                   float3 &parametric,
                   float &backfacing,
                   float &pointiness,
                   float &random_per_island)
{
  /* handle perspective/orthographic */
  incoming = coordinate_incoming(g_data.P);
  position = g_data.P;
  normal = g_data.N;
  true_normal = g_data.Ng;

  if (g_data.is_strand) {
    tangent = g_data.curve_T;
  }
  else {
    tangent_orco_z(orco_attr, orco_attr);
    node_tangent(orco_attr, tangent);
  }

  parametric = float3(g_data.barycentric_coords, 0.0f);
  backfacing = (FrontFacing) ? 0.0f : 1.0f;
  pointiness = 0.5f;
  random_per_island = 0.0f;
}
