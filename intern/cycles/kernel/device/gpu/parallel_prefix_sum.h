/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Parallel prefix sum. */

#include "util/atomic.h"

__device__ void gpu_parallel_prefix_sum(const int global_id,
                                        ccl_global int *ccl_restrict counter,
                                        ccl_global int *ccl_restrict prefix_sum,
                                        const int num_values)
{
  int partition_start = 0;
  int partition_end = num_values;
  int offset = 0;

#ifndef __KERNEL_METAL__
  const ushort local_id = ccl_gpu_thread_idx_x;
  const ushort local_size = ccl_gpu_block_dim_x;

  if (num_values >= local_size) {
    const int partition_size = divide_up(num_values, local_size);
    partition_start = partition_size * local_id;
    partition_end = min(num_values, partition_start + partition_size);

    int total = 0;
    for (int i = partition_start; i < partition_end; i++) {
      total += counter[i];
    }
    prefix_sum[local_id] = total;

    ccl_gpu_syncthreads();

    if (local_id == 0) {
      for (int i = 0; i < local_size; i++) {
        const int new_offset = offset + prefix_sum[i];
        prefix_sum[i] = offset;
        offset = new_offset;
      }
    }

    ccl_gpu_syncthreads();
    offset = prefix_sum[local_id];
    ccl_gpu_syncthreads();
  }
  else
#endif
      if (global_id != 0)
  {
    return;
  }

  for (int i = partition_start; i < partition_end; i++) {
    const int new_offset = offset + counter[i];
    prefix_sum[i] = offset;
    counter[i] = 0;
    offset = new_offset;
  }
}

CCL_NAMESPACE_END
