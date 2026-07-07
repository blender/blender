/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Takes the scene surfel representation to build lists of surfels aligning with a given direction.
 *
 * The lists heads are allocated to fit the surfel granularity.
 *
 * Due to alignment the link and list head are split into several int arrays to avoid too much
 * memory waste.
 *
 * This steps only count the number of surfel per list.
 *
 * Dispatch 1 thread per surfel.
 */

#include "infos/eevee_lightprobe_volume_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_surfel_list_prepare)

#include "eevee_surfel_list_lib.glsl"

void main()
{
  int surfel_id = int(gl_GlobalInvocationID.x);
  if (surfel_id >= int(capture_info_buf.surfel_len)) {
    return;
  }
  float ray_distance;
  int list_id = surfel_list_index_get(
      list_info_buf.ray_grid_size, surfel_buf[surfel_id].position, ray_distance);

  atomicAdd(list_counter_buf[list_id], 1);
  /* Do separate assignment to avoid reference to buffer in arguments which is tricky to cross
   * compile. */
  surfel_buf[surfel_id].ray_distance = ray_distance;
  surfel_buf[surfel_id].list_id = list_id;

  /* Clear for next step. */
  if (gl_GlobalInvocationID.x == 0u) {
    list_info_buf.list_prefix_sum = 0;
  }
}
