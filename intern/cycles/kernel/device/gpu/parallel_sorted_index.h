/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

#pragma once

CCL_NAMESPACE_BEGIN

/* Given an array of states, build an array of indices for which the states
 * are active and sorted by a given key. The prefix sum of the number of active
 * states per key must have already been computed.
 *
 * TODO: there may be ways to optimize this to avoid this many atomic ops? */

#include "util/atomic.h"

#ifdef __HIP__
#  define GPU_PARALLEL_SORTED_INDEX_DEFAULT_BLOCK_SIZE 1024
#else
#  define GPU_PARALLEL_SORTED_INDEX_DEFAULT_BLOCK_SIZE 512
#endif
#define GPU_PARALLEL_SORTED_INDEX_INACTIVE_KEY (~0)

template<typename GetKeyOp>
__device__ void gpu_parallel_sorted_index_array(const uint state_index,
                                                const uint num_states,
                                                const int num_states_limit,
                                                ccl_global int *indices,
                                                ccl_global int *num_indices,
                                                ccl_global int *key_counter,
                                                ccl_global int *key_prefix_sum,
                                                GetKeyOp get_key_op)
{
  const int key = (state_index < num_states) ? get_key_op(state_index) :
                                               GPU_PARALLEL_SORTED_INDEX_INACTIVE_KEY;

  if (key != GPU_PARALLEL_SORTED_INDEX_INACTIVE_KEY) {
    const uint index = atomic_fetch_and_add_uint32(&key_prefix_sum[key], 1);
    if (index < num_states_limit) {
      /* Assign state index. */
      indices[index] = state_index;
    }
    else {
      /* Can't process this state now, increase the counter again so that
       * it will be handled in another iteration. */
      atomic_fetch_and_add_uint32(&key_counter[key], 1);
    }
  }
}

CCL_NAMESPACE_END
