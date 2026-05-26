/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

SHADER_LIBRARY_CREATE_INFO(draw_view)

#include "eevee_surfel_list.bsl.hh"

namespace eevee::surfel {

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[image(0, read_write, SINT_32)]] iimage3DAtomic cluster_list_img;
};

/**
 * Takes scene surfel representation and build list of surfels inside 3D cells.
 *
 * The lists head are allocated to fit the probe samples granularity.
 *
 * Dispatch 1 thread per surfel.
 */
[[compute, local_size(SURFEL_GROUP_SIZE), texture_atomic]]
void build_cluster([[resource_table]] Resources &srt,
                   [[resource_table]] SurfelData &surfels,
                   [[global_invocation_id]] const uint3 global_id,
                   [[local_invocation_id]] const uint3 local_id,
                   [[local_invocation_index]] const uint local_index)
{
  int surfel_index = int(gl_GlobalInvocationID.x);
  if (surfel_index >= int(surfels.capture_info_buf.surfel_len)) {
    return;
  }

  int3 cluster = eevee::surfel::cluster_index_get(
      imageSize(srt.cluster_list_img),
      surfels.capture_info_buf.irradiance_grid_world_to_local,
      surfels.surfel_buf[surfel_index].position);
  /* For debugging. */
  surfels.surfel_buf[surfel_index].cluster_id = cluster.x + cluster.y * 1000 + cluster.z * 1000000;
  /* NOTE: We only need to init the `cluster_list_img` to -1 for the whole list to be valid since
   * every surfel will load its `next` value from the list head. */
  surfels.surfel_buf[surfel_index].next = imageAtomicExchange(
      srt.cluster_list_img, cluster, surfel_index);
}

}  // namespace eevee::surfel

PipelineCompute eevee_surfel_cluster_build(eevee::surfel::build_cluster);
