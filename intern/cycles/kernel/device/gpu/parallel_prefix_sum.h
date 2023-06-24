/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Parallel prefix sum.
 *
 * TODO: actually make this work in parallel.
 *
 * This is used for an array the size of the number of shaders in the scene
 * which is not usually huge, so might not be a significant bottleneck. */

#include "util/atomic.h"

#ifdef __HIP__
#  define GPU_PARALLEL_PREFIX_SUM_DEFAULT_BLOCK_SIZE 1024
#else
#  define GPU_PARALLEL_PREFIX_SUM_DEFAULT_BLOCK_SIZE 512
#endif

__device__ void gpu_parallel_prefix_sum(const int global_id,
                                        ccl_global int *counter,
                                        ccl_global int *prefix_sum,
                                        const int num_values)
{
  if (global_id != 0) {
    return;
  }

  int offset = 0;
  for (int i = 0; i < num_values; i++) {
    const int new_offset = offset + counter[i];
    prefix_sum[i] = offset;
    counter[i] = 0;
    offset = new_offset;
  }
}

CCL_NAMESPACE_END
