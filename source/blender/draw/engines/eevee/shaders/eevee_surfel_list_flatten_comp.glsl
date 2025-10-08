/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Flatten surfel sorting data into a sequential structure.
 * The buffer structure follows the lists OffsetIndices.
 *
 * Dispatched as 1 thread per surfel.
 */

#include "infos/eevee_lightprobe_volume_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_surfel_list_flatten)

void main()
{
  int surfel_id = int(gl_GlobalInvocationID.x);
  if (surfel_id >= int(capture_info_buf.surfel_len)) {
    return;
  }

  int list_id = surfel_buf[surfel_id].list_id;
  int item_id = atomicAdd(list_counter_buf[list_id], -1) - 1;
  item_id += list_range_buf[list_id * 2 + 0];

  list_item_distance_buf[item_id] = surfel_buf[surfel_id].ray_distance;
  list_item_surfel_id_buf[item_id] = surfel_id;
}
