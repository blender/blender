/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Update tagging
 *
 * Any updated shadow caster needs to tag the shadow map tiles it was in and is now into.
 * This is done in 2 pass of this same shader. One for past object bounds and one for new object
 * bounds. The bounding boxes are rasterized and each fragment shader invocation tags the
 * appropriate tiles.
 */

#pragma once
#pragma create_info

#include "eevee_defines.hh"
#include "eevee_shadow_shared.hh"

#include "eevee_shadow_page_ops.bsl.hh"
#include "eevee_shadow_tilemap_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"

namespace eevee::shadow {

struct TagUpdate {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo draw_view_culling;

  [[push_constant]] int tilemap_count;

  [[storage(5, read)]] const ObjectBounds (&bounds_buf)[];
  [[storage(6, read)]] const uint (&resource_ids_buf)[];
};

struct VertIn {
  [[attribute(0)]] float3 pos;
};

struct VertOut {
  [[flat]] uint tilemap_index;
};

[[vertex]]
void tag_update_vert([[resource_table]] TagUpdate &srt,
                     [[resource_table]] TileMaps &tilemaps,
                     [[instance_id]] const int inst_per_tilemap_id,
                     [[in]] const VertIn &v_in,
                     [[out]] VertOut &v_out,
                     [[position]] float4 &out_position)
{
  v_out.tilemap_index = uint(inst_per_tilemap_id) % uint(srt.tilemap_count);
  const uint inst_id = uint(inst_per_tilemap_id) / uint(srt.tilemap_count);
  const uint resource_id = srt.resource_ids_buf[inst_id] & 0x7FFFFFFFu;

  ObjectBounds bounds = srt.bounds_buf[resource_id];
  if (!drw_bounds_are_valid(bounds)) {
    out_position = float4(NAN_FLT);
    return;
  }

  ShadowTileMapData tilemap = tilemaps.tilemaps_buf[v_out.tilemap_index];

  if (is_local_light(tilemap.light_type)) {
    /* Coarse culling to avoid objects beyond the far plane to appear in the flattened projection.
     * This can result in a lot of false positive for small lights. */
    float vs_center_z = transform_point(tilemap.viewmat, bounds.bounding_sphere.xyz).z;
    if (vs_center_z + bounds.bounding_sphere.w < -tilemap.clip_far) {
      out_position = float4(NAN_FLT);
      return;
    }
  }

  const float3 ls_N = v_in.pos;
  /* Convert from -1..1 box shape to 0..1 box. */
  const float3 ls_P = max(float3(0), v_in.pos);

  const float3 P = ls_P.x * bounds.bounding_corners[1].xyz +
                   ls_P.y * bounds.bounding_corners[2].xyz +
                   ls_P.z * bounds.bounding_corners[3].xyz + bounds.bounding_corners[0].xyz;

  /* These normalize should in theory never fail since the bounding boxes are inflated
   * to never be flat. */
  const float3 N = ls_N.x * normalize(float3(bounds.bounding_corners[1].xyz)) +
                   ls_N.y * normalize(float3(bounds.bounding_corners[2].xyz)) +
                   ls_N.z * normalize(float3(bounds.bounding_corners[3].xyz));

  const float3 vs_P = transform_point(tilemap.viewmat, P);
  const float3 vs_N = transform_direction(tilemap.viewmat, N);

  /* Since the aspect ratio is always 1:1 we can use the view normal as the clip space expand
   * direction. */
  const float2 expand_dir = vs_N.xy;

  out_position = tilemap.winmat * float4(vs_P, 1.0f);

  /* To emulate conservative rasterization, we inflate the bounding box by 1 pixel. */
  const float ndc_pixel_size = 2.0f / float(SHADOW_TILEMAP_RES);
  out_position.xy += sign(expand_dir) * (ndc_pixel_size * out_position.w);

  /* Make sure to bring all the geometry inside the view frustum.
   * Mimics an infinite projection matrix. */
  out_position.z *= 1e-5f;
}

[[fragment]]
void tag_update_frag([[resource_table]] Tiles &tiles,
                     [[resource_table]] TileMaps &tilemaps,
                     [[frag_coord]] const float4 frag_coord,
                     [[in]] const VertOut &v_out)
{
  ShadowTileMapData tilemap = tilemaps.tilemaps_buf[v_out.tilemap_index];

  uint2 texel = uint2(frag_coord.xy);
  /* Tag only LOD0. The lower LOD will be written by the tag_propagate pass. */
  int tile_index = shadow_tile_offset(texel, tilemap.tiles_index, 0);
  atomicOr(tiles.tiles_buf[tile_index], uint(SHADOW_DO_UPDATE));
}

/* Propagate the LOD0 update tag to the lower LOD tiles. */
[[compute, local_size(SHADOW_TILEMAP_RES, SHADOW_TILEMAP_RES)]]
void tag_propagate([[resource_table]] Tiles &tiles,
                   [[resource_table]] TileMaps &tilemaps,
                   [[global_invocation_id]] const uint3 global_id)
{
  ShadowTileMapData tilemap = tilemaps.tilemaps_buf[global_id.z];

  uint2 texel = uint2(global_id.xy);
  const int tile_index_lod0 = shadow_tile_offset(texel, tilemap.tiles_index, 0);
  bool do_update = (tiles.tiles_buf[tile_index_lod0] & uint(SHADOW_DO_UPDATE)) != 0;

  if (do_update) {
    /* TODO(fclem): Recursive downsampling. */
    for (int lod = 1; lod <= SHADOW_TILEMAP_LOD; lod++) {
      int tile_index = shadow_tile_offset(texel >> lod, tilemap.tiles_index, lod);
      atomicOr(tiles.tiles_buf[tile_index], uint(SHADOW_DO_UPDATE));
    }
  }
}

PipelineGraphic tag_update(tag_update_vert, tag_update_frag);
PipelineCompute tag_update_propagate(tag_propagate);

}  // namespace eevee::shadow
