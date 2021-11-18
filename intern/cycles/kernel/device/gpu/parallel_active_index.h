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

#ifdef __KERNEL_METAL__
struct ActiveIndexContext {
  ActiveIndexContext(int _thread_index,
                     int _global_index,
                     int _threadgroup_size,
                     int _simdgroup_size,
                     int _simd_lane_index,
                     int _simd_group_index,
                     int _num_simd_groups,
                     threadgroup int *_simdgroup_offset)
      : thread_index(_thread_index),
        global_index(_global_index),
        blocksize(_threadgroup_size),
        ccl_gpu_warp_size(_simdgroup_size),
        thread_warp(_simd_lane_index),
        warp_index(_simd_group_index),
        num_warps(_num_simd_groups),
        warp_offset(_simdgroup_offset)
  {
  }

  const int thread_index, global_index, blocksize, ccl_gpu_warp_size, thread_warp, warp_index,
      num_warps;
  threadgroup int *warp_offset;

  template<uint blocksizeDummy, typename IsActiveOp>
  void active_index_array(const uint num_states,
                          ccl_global int *indices,
                          ccl_global int *num_indices,
                          IsActiveOp is_active_op)
  {
    const uint state_index = global_index;
#else
template<uint blocksize, typename IsActiveOp>
__device__ void gpu_parallel_active_index_array(const uint num_states,
                                                ccl_global int *indices,
                                                ccl_global int *num_indices,
                                                IsActiveOp is_active_op)
{
  extern ccl_gpu_shared int warp_offset[];

  const uint thread_index = ccl_gpu_thread_idx_x;
  const uint thread_warp = thread_index % ccl_gpu_warp_size;

  const uint warp_index = thread_index / ccl_gpu_warp_size;
  const uint num_warps = blocksize / ccl_gpu_warp_size;

  const uint state_index = ccl_gpu_block_idx_x * blocksize + thread_index;
#endif

    /* Test if state corresponding to this thread is active. */
    const uint is_active = (state_index < num_states) ? is_active_op(state_index) : 0;

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
}; /* end class ActiveIndexContext */

/* inject the required thread params into a struct, and redirect to its templated member function
 */
#  define gpu_parallel_active_index_array \
    ActiveIndexContext(metal_local_id, \
                       metal_global_id, \
                       metal_local_size, \
                       simdgroup_size, \
                       simd_lane_index, \
                       simd_group_index, \
                       num_simd_groups, \
                       simdgroup_offset) \
        .active_index_array
#endif

CCL_NAMESPACE_END
