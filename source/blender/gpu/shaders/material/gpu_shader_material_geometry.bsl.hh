/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"
#include "gpu_shader_material_tangent.bsl.hh"

[[node]]
void node_geometry(float3 orco_attr,
                   [[resource_table]] KernelGlobals &kg,
                   ShadingData &sd,
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
  incoming = coordinate_impl(kg, sd, sd.P, sd.N).incoming;
  position = sd.P;
  normal = sd.N;
  true_normal = sd.Ng;

  if (sd.is_strand) {
    tangent = sd.curve_T;
  }
  else {
    tangent_orco_z(orco_attr, orco_attr);
    node_tangent(orco_attr, kg, sd, tangent);
  }

  parametric = float3(sd.barycentric_coords, 0.0f);
  backfacing = (FrontFacing) ? 0.0f : 1.0f;
  pointiness = 0.5f;
  random_per_island = 0.0f;
}
