/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Bounds computation for directional shadows.
 *
 * Iterate through all shadow casters and extract min/max per directional shadow.
 * This needs to happen first in the pipeline to allow tagging all relevant tile-map as dirty if
 * their range changes.
 */
#pragma once

#include "draw_shader_shared.hh"
#include "draw_shape_lib.glsl"
#include "eevee_light_iter_lib.glsl"
#include "eevee_shadow_shared.hh"
#include "gpu_shader_utildefines_lib.glsl"

namespace eevee::shadow {

struct TilemapBoundsInit {
  [[storage(0, write)]] ShadowTileMapClip (&tilemaps_clip_buf)[];
  [[push_constant]] const int tilemaps_clip_buf_len;
};

[[compute, local_size(SHADOW_CLIPMAP_GROUP_SIZE)]]
void tilemap_bounds_clear([[resource_table]] TilemapBoundsInit &srt,
                          [[global_invocation_id]] const uint3 global_id)
{
  int index = int(global_id.x);
  if (index < srt.tilemaps_clip_buf_len) {
    srt.tilemaps_clip_buf[index].clip_far = floatBitsToOrderedInt(-FLT_MAX);
    srt.tilemaps_clip_buf[index].clip_near = floatBitsToOrderedInt(FLT_MAX);
  }
}

struct TilemapBounds {
  [[storage(LIGHT_CULL_BUF_SLOT, read)]] const LightCullingData &light_cull_buf;
  [[storage(LIGHT_BUF_SLOT, read_write)]] LightData (&light_buf)[];
  [[storage(4, read)]] const uint (&casters_id_buf)[];
  [[storage(5, read_write)]] ShadowTileMapData (&tilemaps_buf)[];
  [[storage(6, read)]] const ObjectBounds (&bounds_buf)[];
  [[storage(7, read_write)]] ShadowTileMapClip (&tilemaps_clip_buf)[];

  [[push_constant]] const int resource_len;

  [[shared]] int global_min;
  [[shared]] int global_max;
};

[[compute, local_size(SHADOW_BOUNDS_GROUP_SIZE)]]
void tilemap_bounds_min_max([[resource_table]] TilemapBounds &srt,
                            [[global_invocation_id]] const uint3 global_id,
                            [[local_invocation_index]] const uint local_index)
{
  Box box;
  bool is_valid = true;

  if (srt.resource_len > 0) {
    uint index = global_id.x;
    /* Keep uniform control flow. Do not return. */
    index = min(index, uint(srt.resource_len) - 1);
    uint resource_id = srt.casters_id_buf[index];
    resource_id = (resource_id & 0x7FFFFFFFu);

    ObjectBounds bounds = srt.bounds_buf[resource_id];
    is_valid = drw_bounds_corners_are_valid(bounds);
    box = shape_box(bounds.bounding_corners[0].xyz,
                    bounds.bounding_corners[0].xyz + bounds.bounding_corners[1].xyz,
                    bounds.bounding_corners[0].xyz + bounds.bounding_corners[2].xyz,
                    bounds.bounding_corners[0].xyz + bounds.bounding_corners[3].xyz);
  }
  else {
    /* Create a dummy box so initialization happens even when there are no shadow casters. */
    box = shape_box(float3(-1.0f),
                    float3(-1.0f) + float3(1.0f, 0.0f, 0.0f),
                    float3(-1.0f) + float3(0.0f, 1.0f, 0.0f),
                    float3(-1.0f) + float3(0.0f, 0.0f, 1.0f));
  }

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (srt.light_cull_buf, l_idx) {
    LightData light = srt.light_buf[l_idx];

    if (light.tilemap_index == LIGHT_NO_SHADOW) {
      continue;
    }

    float local_min = FLT_MAX;
    float local_max = -FLT_MAX;
    for (int i = 0; i < 8; i++) {
      float z = dot(box.corners[i].xyz, -light_z_axis(light));
      local_min = min(local_min, z);
      local_max = max(local_max, z);
    }

    if (local_index == 0u) {
      srt.global_min = floatBitsToOrderedInt(FLT_MAX);
      srt.global_max = floatBitsToOrderedInt(-FLT_MAX);
    }

    barrier();

    /* Quantization bias. */
    local_min -= abs(local_min) * 0.01f;
    local_max += abs(local_max) * 0.01f;

    if (is_valid) {
      /* Intermediate result. Min/Max of a compute group. */
      atomicMin(srt.global_min, floatBitsToOrderedInt(local_min));
      atomicMax(srt.global_max, floatBitsToOrderedInt(local_max));
    }

    barrier();

    if (local_index == 0u) {
      /* Final result. Min/Max of the whole dispatch. */
      atomicMin(srt.light_buf[l_idx].clip_near, srt.global_min);
      atomicMax(srt.light_buf[l_idx].clip_far, srt.global_max);
      /* TODO(fclem): This feel unnecessary but we currently have no indexing from
       * tile-map to lights. This is because the lights are selected by culling phase. */
      for (int i = light.tilemap_index; i <= light_tilemap_max_get(light); i++) {
        int index = srt.tilemaps_buf[i].clip_data_index;
        atomicMin(srt.tilemaps_clip_buf[index].clip_near, srt.global_min);
        atomicMax(srt.tilemaps_clip_buf[index].clip_far, srt.global_max);
      }
    }

    /* No need for barrier here since global_min/max is only read by thread 0 before being reset by
     * thread 0. */
  }
  LIGHT_FOREACH_END
}

PipelineCompute tilemap_bounds_init(tilemap_bounds_clear);
PipelineCompute tilemap_bounds(tilemap_bounds_min_max);

}  // namespace eevee::shadow
