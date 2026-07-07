/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Create a prefix sum of the surfels per list.
 * Outputs one IndexRange for each surfel list.
 *
 * Dispatched as 1 thread per surfel list.
 */

#include "infos/eevee_lightprobe_volume_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_surfel_list_prefix)

void main()
{
  int list_id = int(gl_GlobalInvocationID.x);
  if (list_id >= list_info_buf.list_max) {
    return;
  }

  int list_item_count = list_counter_buf[list_id];
  int list_item_start = atomicAdd(list_info_buf.list_prefix_sum, list_item_count);

  list_range_buf[list_id * 2 + 0] = list_item_start;
  list_range_buf[list_id * 2 + 1] = list_item_count;
}
