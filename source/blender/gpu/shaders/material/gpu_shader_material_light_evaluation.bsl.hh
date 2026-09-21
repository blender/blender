/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void node_light_evaluation_common(const float light_index,
                                  float3 position,
                                  [[resource_table]] KernelGlobals &kg,
                                  float &mask,
                                  float3 &direction,
                                  float &distance)
{
  node_light_evaluation_common_impl(kg, int(light_index), position, direction, distance, mask);
}

[[node]]
void node_light_evaluation_diffuse(const float light_index,
                                   float3 position,
                                   float3 normal,
                                   float roughness,
                                   [[resource_table]] KernelGlobals &kg,
                                   const ShadingData &sd,
                                   float &factor,
                                   float &mask,
                                   float3 &direction,
                                   float &distance)
{
  node_light_evaluation_common_impl(kg, int(light_index), position, direction, distance, mask);
  node_light_evaluation_impl<true>(kg, sd, int(light_index), position, normal, roughness, factor);
}

[[node]]
void node_light_evaluation_glossy(const float light_index,
                                  float3 position,
                                  float3 normal,
                                  float roughness,
                                  [[resource_table]] KernelGlobals &kg,
                                  const ShadingData &sd,
                                  float &factor,
                                  float &mask,
                                  float3 &direction,
                                  float &distance)
{
  node_light_evaluation_common_impl(kg, int(light_index), position, direction, distance, mask);
  node_light_evaluation_impl<false>(kg, sd, int(light_index), position, normal, roughness, factor);
}
