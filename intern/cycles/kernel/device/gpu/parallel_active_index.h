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

/* Given an array of states, build an array of indices for which the states
 * are active.
 *
 * Shared memory requirement is `sizeof(int) * (number_of_warps + 1)`. */

#include "util/atomic.h"

#ifdef __HIP__
#  define GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE 1024
#else
#  define GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE 512
#endif

#ifndef __KERNEL_METAL__
template<uint blocksize, typename IsActiveOp>
__device__
#endif
void gpu_parallel_active_index_array_impl(const uint num_states,
                                          ccl_global int *indices,
                                          ccl_global int *num_indices,
#ifdef __KERNEL_METAL__
                                          const uint is_active,
                                          const uint blocksize,
                                          const int thread_index,
                                          const uint state_index,
                                          const int ccl_gpu_warp_size,
                                          const int thread_warp,
                                          const int warp_index,
                                          const int num_warps,
                                          threadgroup int *warp_offset)
{
#else
                                          IsActiveOp is_active_op)
{
  extern ccl_gpu_shared int warp_offset[];

  const uint thread_index = ccl_gpu_thread_idx_x;
  const uint thread_warp = thread_index % ccl_gpu_warp_size;

  const uint warp_index = thread_index / ccl_gpu_warp_size;
  const uint num_warps = blocksize / ccl_gpu_warp_size;

  const uint state_index = ccl_gpu_block_idx_x * blocksize + thread_index;

  /* Test if state corresponding to this thread is active. */
  const uint is_active = (state_index < num_states) ? is_active_op(state_index) : 0;
#endif

  /* For each thread within a warp compute how many other active states precede it. */
  const uint thread_offset = popcount(ccl_gpu_ballot(is_active) &
                                      ccl_gpu_thread_mask(thread_warp));

  /* Last thread in warp stores number of active states for each warp. */
  if (thread_warp == ccl_gpu_warp_size - 1) {
    warp_offset[warp_index] = thread_offset + is_active;
  }

  ccl_gpu_syncthreads();

  /* Last thread in block converts per-warp sizes to offsets, increments global size of
    * index array and gets offset to write to. */
  if (thread_index == blocksize - 1) {
    /* TODO: parallelize this. */
    int offset = 0;
    for (int i = 0; i < num_warps; i++) {
      int num_active = warp_offset[i];
      warp_offset[i] = offset;
      offset += num_active;
    }

    const uint block_num_active = warp_offset[warp_index] + thread_offset + is_active;
    warp_offset[num_warps] = atomic_fetch_and_add_uint32(num_indices, block_num_active);
  }

  ccl_gpu_syncthreads();

  /* Write to index array. */
  if (is_active) {
    const uint block_offset = warp_offset[num_warps];
    indices[block_offset + warp_offset[warp_index] + thread_offset] = state_index;
  }
}

#ifdef __KERNEL_METAL__

#  define gpu_parallel_active_index_array(dummy, num_states, indices, num_indices, is_active_op) \
  const uint is_active = (ccl_gpu_global_id_x() < num_states) ? is_active_op(ccl_gpu_global_id_x()) : 0; \
  gpu_parallel_active_index_array_impl(num_states, indices, num_indices, is_active, \
    metal_local_size, metal_local_id, metal_global_id, simdgroup_size, simd_lane_index, \
    simd_group_index, num_simd_groups, simdgroup_offset)

#else

#  define gpu_parallel_active_index_array(blocksize, num_states, indices, num_indices, is_active_op) \
  gpu_parallel_active_index_array_impl<blocksize>(num_states, indices, num_indices, is_active_op)

#endif

CCL_NAMESPACE_END
