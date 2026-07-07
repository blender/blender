/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Sort the lights by their Z distance to the camera.
 * Outputs ordered light buffer.
 * One thread processes one Light entity.
 */

#include "infos/eevee_light_culling_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_light_culling_sort)

#include "gpu_shader_math_base_lib.glsl"

shared float zdists_cache[gl_WorkGroupSize.x];

void main()
{
  /* Early exit if no lights are present to prevent out of bounds buffer read. */
  if (light_cull_buf.visible_count == 0) {
    return;
  }

  uint src_index = gl_GlobalInvocationID.x;
  bool valid_thread = true;

  if (src_index >= light_cull_buf.visible_count) {
    /* Do not return because we use barriers later on (which need uniform control flow).
     * Just process the same last item but avoid insertion. */
    src_index = light_cull_buf.visible_count - 1;
    valid_thread = false;
  }

  float local_zdist = in_zdist_buf[src_index];

  int prefix_sum = 0;
  /* Iterate over the whole key buffer. */
  uint iter = divide_ceil(light_cull_buf.visible_count, gl_WorkGroupSize.x);
  for (uint i = 0u; i < iter; i++) {
    uint index = gl_WorkGroupSize.x * i + gl_LocalInvocationID.x;
    /* NOTE: This will load duplicated values, but they will be discarded. */
    index = min(index, light_cull_buf.visible_count - 1);
    zdists_cache[gl_LocalInvocationID.x] = in_zdist_buf[index];

    barrier();

    /* Iterate over the cache line. */
    uint line_end = min(gl_WorkGroupSize.x, light_cull_buf.visible_count - gl_WorkGroupSize.x * i);
    for (uint j = 0u; j < line_end; j++) {
      if (zdists_cache[j] < local_zdist) {
        prefix_sum++;
      }
      else if (zdists_cache[j] == local_zdist) {
        /* Same depth, use index to order and avoid same prefix for 2 different lights. */
        if ((gl_WorkGroupSize.x * i + j) < src_index) {
          prefix_sum++;
        }
      }
    }

    barrier();
  }

  if (valid_thread) {
    /* Copy sorted light to render light buffer. */
    uint input_index = in_key_buf[src_index];
    out_light_buf[prefix_sum] = in_light_buf[input_index];
  }
}
