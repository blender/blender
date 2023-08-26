/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_material_tangent.glsl)

void node_geometry(vec3 orco_attr,
                   out vec3 position,
                   out vec3 normal,
                   out vec3 tangent,
                   out vec3 true_normal,
                   out vec3 incoming,
                   out vec3 parametric,
                   out float backfacing,
                   out float pointiness,
                   out float random_per_island)
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

  parametric = vec3(g_data.barycentric_coords, 0.0);
  backfacing = (FrontFacing) ? 0.0 : 1.0;
  pointiness = 0.5;
  random_per_island = 0.0;
}
