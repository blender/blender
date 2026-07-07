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

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_shadow_tilemap_bounds)

#include "draw_shape_lib.glsl"
#include "eevee_light_iter_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

shared int global_min;
shared int global_max;

void main()
{
  Box box;
  bool is_valid = true;

  if (resource_len > 0) {
    uint index = gl_GlobalInvocationID.x;
    /* Keep uniform control flow. Do not return. */
    index = min(index, uint(resource_len) - 1);
    uint resource_id = casters_id_buf[index];
    resource_id = (resource_id & 0x7FFFFFFFu);

    ObjectBounds bounds = bounds_buf[resource_id];
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

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    LightData light = light_buf[l_idx];

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

    if (gl_LocalInvocationIndex == 0u) {
      global_min = floatBitsToOrderedInt(FLT_MAX);
      global_max = floatBitsToOrderedInt(-FLT_MAX);
    }

    barrier();

    /* Quantization bias. */
    local_min -= abs(local_min) * 0.01f;
    local_max += abs(local_max) * 0.01f;

    if (is_valid) {
      /* Intermediate result. Min/Max of a compute group. */
      atomicMin(global_min, floatBitsToOrderedInt(local_min));
      atomicMax(global_max, floatBitsToOrderedInt(local_max));
    }

    barrier();

    if (gl_LocalInvocationIndex == 0u) {
      /* Final result. Min/Max of the whole dispatch. */
      atomicMin(light_buf[l_idx].clip_near, global_min);
      atomicMax(light_buf[l_idx].clip_far, global_max);
      /* TODO(fclem): This feel unnecessary but we currently have no indexing from
       * tile-map to lights. This is because the lights are selected by culling phase. */
      for (int i = light.tilemap_index; i <= light_tilemap_max_get(light); i++) {
        int index = tilemaps_buf[i].clip_data_index;
        atomicMin(tilemaps_clip_buf[index].clip_near, global_min);
        atomicMax(tilemaps_clip_buf[index].clip_far, global_max);
      }
    }

    /* No need for barrier here since global_min/max is only read by thread 0 before being reset by
     * thread 0. */
  }
  LIGHT_FOREACH_END
}
