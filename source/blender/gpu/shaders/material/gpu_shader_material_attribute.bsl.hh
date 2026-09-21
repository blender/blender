/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_common_math.bsl.hh"
#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void node_attribute_color(float4 attr, float4 &out_attr)
{
  out_attr = attr_load_color_post(attr);
}

[[node]]
void node_attribute_temperature(float4 attr, float4 &out_attr)
{
  float temperature = attr_load_temperature_post(attr.x);
  out_attr.x = temperature;
  out_attr.y = temperature;
  out_attr.z = temperature;
  out_attr.w = 1.0f;
}

[[node]]
void node_attribute_radiance(float4 attr, float4 &out_attr)
{
  out_attr = attr_load_radiance_post(attr);
}

[[node]]
void node_attribute_density(float4 attr, float &out_attr)
{
  out_attr = attr.x;
}

[[node]]
void node_attribute_flame(float4 attr, float &out_attr)
{
  out_attr = attr.x;
}

[[node]]
void node_attribute_uniform(float4 attr,
                            const float attr_hash,
                            [[resource_table]] KernelGlobals &kg,
                            const ShadingData &sd,
                            float4 &out_attr)
{
  /* Temporary solution to support both old UBO attributes and new SSBO loading.
   * Old UBO load is already done through `attr` and will just be passed through. */
  out_attr = attr_load_uniform(kg, sd, attr, floatBitsToUint(attr_hash));
}

[[node]]
void node_attribute_light_is_sun(const float light_index,
                                 [[resource_table]] KernelGlobals &kg,
                                 float4 &out_attr)
{
  out_attr = float4(node_attribute_light_is_sun_impl(kg, int(light_index)));
}

[[node]]
void node_attribute_light_is_point(const float light_index,
                                   [[resource_table]] KernelGlobals &kg,
                                   float4 &out_attr)
{
  out_attr = float4(node_attribute_light_is_point_impl(kg, int(light_index)));
}

[[node]]
void node_attribute_light_is_spot(const float light_index,
                                  [[resource_table]] KernelGlobals &kg,
                                  float4 &out_attr)
{
  out_attr = float4(node_attribute_light_is_spot_impl(kg, int(light_index)));
}

[[node]]
void node_attribute_light_is_area(const float light_index,
                                  [[resource_table]] KernelGlobals &kg,
                                  float4 &out_attr)
{
  out_attr = float4(node_attribute_light_is_area_impl(kg, int(light_index)));
}

[[node]]
void node_attribute_light_cutoff_distance(const float light_index,
                                          [[resource_table]] KernelGlobals &kg,
                                          float4 &out_attr)
{
  out_attr = float4(node_attribute_light_cutoff_distance_impl(kg, int(light_index)));
}

[[node]]
void node_attribute_light(const float light_index,
                          const float attr_hash,
                          [[resource_table]] KernelGlobals &kg,
                          float4 &out_attr)
{
  out_attr = node_attribute_light_impl(kg, int(light_index), floatBitsToUint(attr_hash));
}

float4 attr_load_layer([[resource_table]] KernelGlobals &kg, const uint attr_hash)
{
  return node_attribute_layer_impl(kg, attr_hash);
}

[[node]]
void node_attribute(float4 attr, float4 &outcol, float3 &outvec, float &outf, float &outalpha)
{
  outcol = float4(attr.xyz, 1.0f);
  outvec = attr.xyz;
  outf = math_average(attr.xyz);
  outalpha = attr.w;
}
