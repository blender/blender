/*
 * Copyright 2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Parallel prefix sum.
 *
 * TODO: actually make this work in parallel.
 *
 * This is used for an array the size of the number of shaders in the scene
 * which is not usually huge, so might not be a significant bottleneck. */

#include "util/util_atomic.h"

#ifdef __HIP__
#  define GPU_PARALLEL_PREFIX_SUM_DEFAULT_BLOCK_SIZE 1024
#else
#  define GPU_PARALLEL_PREFIX_SUM_DEFAULT_BLOCK_SIZE 512
#endif

template<uint blocksize>
__device__ void gpu_parallel_prefix_sum(int *counter, int *prefix_sum, const int num_values)
{
  if (!(ccl_gpu_block_idx_x == 0 && ccl_gpu_thread_idx_x == 0)) {
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
