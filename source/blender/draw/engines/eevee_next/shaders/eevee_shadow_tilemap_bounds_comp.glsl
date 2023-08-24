/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadowmapping: Bounds computation for directional shadows.
 *
 * Iterate through all shadow casters and extract min/max per directional shadow.
 * This needs to happen first in the pipeline to allow tagging all relevant tilemap as dirty if
 * their range changes.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_intersect_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_tilemap_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_iter_lib.glsl)

shared int global_min;
shared int global_max;

void main()
{
  uint index = gl_GlobalInvocationID.x;
  /* Keep uniform control flow. Do not return. */
  index = min(index, uint(resource_len) - 1);

  uint resource_id = casters_id_buf[index];
  ObjectBounds bounds = bounds_buf[resource_id];
  IsectBox box = isect_data_setup(bounds.bounding_corners[0].xyz,
                                  bounds.bounding_corners[1].xyz,
                                  bounds.bounding_corners[2].xyz,
                                  bounds.bounding_corners[3].xyz);

  LIGHT_FOREACH_BEGIN_DIRECTIONAL (light_cull_buf, l_idx) {
    LightData light = light_buf[l_idx];

    float local_min = FLT_MAX;
    float local_max = -FLT_MAX;
    for (int i = 0; i < 8; i++) {
      float z = dot(box.corners[i].xyz, -light._back);
      local_min = min(local_min, z);
      local_max = max(local_max, z);
    }

    if (gl_LocalInvocationID.x == 0) {
      global_min = floatBitsToOrderedInt(FLT_MAX);
      global_max = floatBitsToOrderedInt(-FLT_MAX);
    }

    barrier();

    /* Quantization bias. */
    local_min -= abs(local_min) * 0.01;
    local_max += abs(local_max) * 0.01;

    /* Intermediate result. Min/Max of a compute group. */
    atomicMin(global_min, floatBitsToOrderedInt(local_min));
    atomicMax(global_max, floatBitsToOrderedInt(local_max));

    barrier();

    if (gl_LocalInvocationID.x == 0) {
      /* Final result. Min/Max of the whole dispatch. */
      atomicMin(light_buf[l_idx].clip_near, global_min);
      atomicMax(light_buf[l_idx].clip_far, global_max);
      /* TODO(fclem): This feel unecessary but we currently have no indexing from
       * tilemap to lights. This is because the lights are selected by culling phase. */
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
