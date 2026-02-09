/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

bool tiled_image_lookup(float3 &co, sampler2DArray ima, sampler1DArray map)
{
  float2 tile_pos = floor(co.xy);

  if (tile_pos.x < 0 || tile_pos.y < 0 || tile_pos.x >= 10) {
    return false;
  }
  float tile = 10 * tile_pos.y + tile_pos.x;
  if (tile >= textureSize(map, 0).x) {
    return false;
  }
  /* Fetch tile information. */
  float tile_layer = texelFetch(map, int2(tile, 0), 0).x;
  if (tile_layer < 0) {
    return false;
  }
  float4 tile_info = texelFetch(map, int2(tile, 1), 0);

  co = float3(((co.xy - tile_pos) * tile_info.zw) + tile_info.xy, tile_layer);
  return true;
}
