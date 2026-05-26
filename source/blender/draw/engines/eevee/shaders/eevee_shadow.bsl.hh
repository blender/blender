/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_sampling_lib.bsl.hh"
#include "eevee_shadow_tilemap_lib.bsl.hh"
#include "eevee_uniform.bsl.hh"
#include "eevee_utility_tx.bsl.hh"
#include "gpu_shader_utildefines_lib.glsl"

namespace eevee {

/* Any entry point function using this should also use `[[texture_atomic]]`. */
struct ShadowRenderData {
  [[sampler(SHADOW_ATLAS_TEX_SLOT)]] usampler2DArrayAtomic shadow_atlas_tx;
  [[sampler(SHADOW_TILEMAPS_TEX_SLOT)]] usampler2D shadow_tilemaps_tx;

  [[compilation_constant]] bool shadow_random;

  [[resource_table]] srt_t<Uniform> uniforms;
  [[resource_table, condition(shadow_random)]] srt_t<Sampling> sampling;
  [[resource_table, condition(shadow_random)]] srt_t<UtilityTexture> util_tx;

  float read_depth(ShadowCoordinates coord) const
  {
    ShadowSamplingTile tile = shadow_tile_load(
        shadow_tilemaps_tx, coord.tilemap_tile, coord.tilemap_index);
    if (!tile.is_valid) {
      return -1.0f;
    }
    /* Using bitwise ops is way faster than integer ops. */
    constexpr uint page_shift = uint(SHADOW_PAGE_LOD);
    constexpr uint page_mask = ~(0xFFFFFFFFu << uint(SHADOW_PAGE_LOD));

    uint2 texel = coord.tilemap_texel;
    /* Shift LOD0 pixels so that they get wrapped at the right position for the given LOD. */
    texel += uint2(tile.lod_offset << SHADOW_PAGE_LOD);
    /* Scale to LOD pixels (merge LOD0 pixels together) then mask to get pixel in page. */
    uint2 texel_page = (texel >> tile.lod) & page_mask;
    texel = (uint2(tile.page.xy) << page_shift) | texel_page;

    return uintBitsToFloat(texelFetch(shadow_atlas_tx, int3(int2(texel), int(tile.page.z)), 0).r);
  }

  float punctual_sample_get(LightData light, float3 P) const
  {
    float3 shadow_position = light.local().local.shadow_position;
    float3 lP = transform_point_inversed(light.object_to_world, P);
    lP -= shadow_position;
    int face_id = shadow_punctual_face_index_get(lP);
    lP = shadow_punctual_local_position_to_face_local(face_id, lP);
    ShadowCoordinates coord = shadow_punctual_coordinates(light, lP, face_id);

    float radial_dist = read_depth(coord);
    if (radial_dist == -1.0f) {
      return 1e10f;
    }
    float receiver_dist = length(lP);
    float occluder_dist = radial_dist;
    return receiver_dist - occluder_dist;
  }

  float directional_sample_get(LightData light, float3 P) const
  {
    float3 lP = transform_direction_transposed(light.object_to_world, P);
    ShadowCoordinates coord = shadow_directional_coordinates(light, lP);

    float depth = read_depth(coord);
    if (depth == -1.0f) {
      return 1e10f;
    }
    /* Use increasing distance from the light. */
    float receiver_dist = -lP.z - orderedIntBitsToFloat(light.clip_near);
    float occluder_dist = depth;
    return receiver_dist - occluder_dist;
  }

  float shadow_sample(const bool is_directional, LightData light, float3 P) const
  {
    if (is_directional) {
      return directional_sample_get(light, P);
    }
    return punctual_sample_get(light, P);
  }
};

}  // namespace eevee
