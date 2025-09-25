/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Takes scene surfel representation and build list of surfels inside 3D cells.
 *
 * The lists head are allocated to fit the probe samples granularity.
 *
 * Dispatch 1 thread per surfel.
 */

#include "infos/eevee_lightprobe_volume_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_surfel_cluster_build)

#include "eevee_surfel_list_lib.glsl"

void main()
{
  int surfel_index = int(gl_GlobalInvocationID.x);
  if (surfel_index >= int(capture_info_buf.surfel_len)) {
    return;
  }

  int3 cluster = surfel_cluster_index_get(imageSize(cluster_list_img),
                                          capture_info_buf.irradiance_grid_world_to_local,
                                          surfel_buf[surfel_index].position);
  /* For debugging. */
  surfel_buf[surfel_index].cluster_id = cluster.x + cluster.y * 1000 + cluster.z * 1000000;
  /* NOTE: We only need to init the `cluster_list_img` to -1 for the whole list to be valid since
   * every surfel will load its `next` value from the list head. */
  surfel_buf[surfel_index].next = imageAtomicExchange(cluster_list_img, cluster, surfel_index);
}
