/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once
#pragma create_info

#include "gpu_shader_compat.hh"

namespace workbench::color {

struct Materials {
  [[storage(WB_MATERIAL_SLOT, read)]] float4 (&materials_data)[];

  void material_data_get(int handle,
                         float3 vertex_color,
                         float3 &color,
                         float &alpha,
                         float &roughness,
                         float &metallic)
  {
    float4 data = materials_data[handle];
    color = (data.r == -1) ? vertex_color : data.rgb;

    uint encoded_data = floatBitsToUint(data.w);
    alpha = float((encoded_data >> 16u) & 0xFFu) * (1.0f / 255.0f);
    roughness = float((encoded_data >> 8u) & 0xFFu) * (1.0f / 255.0f);
    metallic = float(encoded_data & 0xFFu) * (1.0f / 255.0f);
  }
};

}  // namespace workbench::color
