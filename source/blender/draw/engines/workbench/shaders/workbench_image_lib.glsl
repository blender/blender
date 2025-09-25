/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/workbench_prepass_infos.hh"

SHADER_LIBRARY_CREATE_INFO(workbench_color_texture)

/* TODO(fclem): deduplicate code. */
bool node_tex_tile_lookup(inout float3 co, sampler1DArray map)
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
  float tile_layer = texelFetch(map, int2(tile, 0), 0).x;
  if (tile_layer < 0.0f) {
    return false;
  }

  float4 tile_info = texelFetch(map, int2(tile, 1), 0);

  co = float3(((co.xy - tile_pos) * tile_info.zw) + tile_info.xy, tile_layer);
  return true;
}

float3 workbench_image_color(float2 uvs)
{
#ifdef WORKBENCH_COLOR_TEXTURE
  float4 color;

  float3 co = float3(uvs, 0.0f);
  if (is_image_tile) {
    if (node_tex_tile_lookup(co, imageTileData)) {
      color = texture(imageTileArray, co);
    }
    else {
      color = float4(1.0f, 0.0f, 1.0f, 1.0f);
    }
  }
  else {
    color = texture(imageTexture, uvs);
  }

  /* Unpremultiply if stored multiplied, since straight alpha is expected by shaders. */
  if (image_premult && !(color.a == 0.0f || color.a == 1.0f)) {
    color.rgb /= color.a;
  }

#  ifdef GPU_FRAGMENT_SHADER
  if (color.a < image_transparency_cutoff) {
    gpu_discard_fragment();
  }
#  endif

  return color.rgb;
#else

  return float3(1.0f);
#endif
}
