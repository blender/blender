/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once
#pragma create_info

#include "gpu_shader_compat.hh"

namespace workbench::color {

struct Texture {
  [[sampler(WB_TEXTURE_SLOT) /*, frequency(batch)*/]] sampler2D imageTexture;
  [[sampler(WB_TILE_ARRAY_SLOT) /*, frequency(batch)*/]] sampler2DArray imageTileArray;
  [[sampler(WB_TILE_DATA_SLOT) /*, frequency(batch)*/]] sampler1DArray imageTileData;
  [[push_constant]] bool is_image_tile;
  [[push_constant]] bool image_premult;
  [[push_constant]] float image_transparency_cutoff;
};

/* TODO(fclem): deduplicate code. */
bool tile_lookup(float3 &co, sampler1DArray map)
{
  float2 tile_pos = floor(co.xy);

  if (tile_pos.x < 0 || tile_pos.y < 0 || tile_pos.x >= 10) {
    return false;
  }

  float tile = 10.0f * tile_pos.y + tile_pos.x;
  if (tile >= textureSize(map, 0).x) {
    return false;
  }

  /* Fetch tile information. */
  float tile_layer = texelFetch(map, int2(int(tile), 0), 0).x;
  if (tile_layer < 0.0f) {
    return false;
  }

  float4 tile_info = texelFetch(map, int2(int(tile), 1), 0);

  co = float3(((co.xy - tile_pos) * tile_info.zw) + tile_info.xy, tile_layer);
  return true;
}

float3 image_color([[resource_table]] Texture &srt, float2 uvs)
{
  float4 color;

  float3 co = float3(uvs, 0.0f);
  if (srt.is_image_tile) {
    if (tile_lookup(co, srt.imageTileData)) {
      color = texture(srt.imageTileArray, co);
    }
    else {
      color = float4(1.0f, 0.0f, 1.0f, 1.0f);
    }
  }
  else {
    color = texture(srt.imageTexture, uvs);
  }

  /* Un-pre-multiply if stored multiplied, since straight alpha is expected by shaders. */
  if (srt.image_premult && !(color.a == 0.0f || color.a == 1.0f)) {
    color.rgb /= color.a;
  }

  if (color.a < srt.image_transparency_cutoff) {
    gpu_discard_fragment();
  }

  return color.rgb;
}

}  // namespace workbench::color
