/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/workbench_prepass_infos.hh"

// SHADER_LIBRARY_CREATE_INFO(workbench_color_material)
SHADER_LIBRARY_CREATE_INFO(workbench_color_texture)

void workbench_material_data_get(int handle,
                                 float3 vertex_color,
                                 out float3 color,
                                 out float alpha,
                                 out float roughness,
                                 out float metallic)
{
#ifdef WORKBENCH_COLOR_MATERIAL
  float4 data = materials_data[handle];
#else
  float4 data = float4(0.0f);
#endif
  color = (data.r == -1) ? vertex_color : data.rgb;

  uint encoded_data = floatBitsToUint(data.w);
  alpha = float((encoded_data >> 16u) & 0xFFu) * (1.0f / 255.0f);
  roughness = float((encoded_data >> 8u) & 0xFFu) * (1.0f / 255.0f);
  metallic = float(encoded_data & 0xFFu) * (1.0f / 255.0f);
}
