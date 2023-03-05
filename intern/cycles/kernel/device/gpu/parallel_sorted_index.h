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
#define GPU_PARALLEL_SORT_BLOCK_SIZE 1024

#if defined(__KERNEL_LOCAL_ATOMIC_SORT__)

#  define atomic_store_local(p, x) \
    atomic_store_explicit((threadgroup atomic_int *)p, x, memory_order_relaxed)
#  define atomic_load_local(p) \
    atomic_load_explicit((threadgroup atomic_int *)p, memory_order_relaxed)

ccl_device_inline void gpu_parallel_sort_bucket_pass(const uint num_states,
                                                     const uint partition_size,
                                                     const uint max_shaders,
                                                     const uint queued_kernel,
                                                     ccl_global ushort *d_queued_kernel,
                                                     ccl_global uint *d_shader_sort_key,
                                                     ccl_global int *partition_key_offsets,
                                                     ccl_gpu_shared int *buckets,
                                                     const ushort local_id,
                                                     const ushort local_size,
                                                     const ushort grid_id)
{
  /* Zero the bucket sizes. */
  if (local_id < max_shaders) {
    atomic_store_local(&buckets[local_id], 0);
  }

  ccl_gpu_syncthreads();

  /* Determine bucket sizes within the partitions. */

  const uint partition_start = partition_size * uint(grid_id);
  const uint partition_end = min(num_states, partition_start + partition_size);

  for (int state_index = partition_start + uint(local_id); state_index < partition_end;
       state_index += uint(local_size)) {
    ushort kernel_index = d_queued_kernel[state_index];
    if (kernel_index == queued_kernel) {
      uint key = d_shader_sort_key[state_index] % max_shaders;
      atomic_fetch_and_add_uint32(&buckets[key], 1);
    }
  }

  ccl_gpu_syncthreads();

  /* Calculate the partition's local offsets from the prefix sum of bucket sizes. */

  if (local_id == 0) {
    int offset = 0;
    for (int i = 0; i < max_shaders; i++) {
      partition_key_offsets[i + uint(grid_id) * (max_shaders + 1)] = offset;
      offset = offset + atomic_load_local(&buckets[i]);
    }

    /* Store the number of active states in this partition. */
    partition_key_offsets[max_shaders + uint(grid_id) * (max_shaders + 1)] = offset;
  }
}

ccl_device_inline void gpu_parallel_sort_write_pass(const uint num_states,
                                                    const uint partition_size,
                                                    const uint max_shaders,
                                                    const uint queued_kernel,
                                                    const int num_states_limit,
                                                    ccl_global int *indices,
                                                    ccl_global ushort *d_queued_kernel,
                                                    ccl_global uint *d_shader_sort_key,
                                                    ccl_global int *partition_key_offsets,
                                                    ccl_gpu_shared int *local_offset,
                                                    const ushort local_id,
                                                    const ushort local_size,
                                                    const ushort grid_id)
{
  /* Calculate each partition's global offset from the prefix sum of the active state counts per
   * partition. */

  if (local_id < max_shaders) {
    int partition_offset = 0;
    for (int i = 0; i < uint(grid_id); i++) {
      int partition_key_count = partition_key_offsets[max_shaders + uint(i) * (max_shaders + 1)];
      partition_offset += partition_key_count;
    }

    ccl_global int *key_offsets = partition_key_offsets + (uint(grid_id) * (max_shaders + 1));
    atomic_store_local(&local_offset[local_id], key_offsets[local_id] + partition_offset);
  }

  ccl_gpu_syncthreads();

  /* Write the sorted active indices. */

  const uint partition_start = partition_size * uint(grid_id);
  const uint partition_end = min(num_states, partition_start + partition_size);

  ccl_global int *key_offsets = partition_key_offsets + (uint(grid_id) * max_shaders);

  for (int state_index = partition_start + uint(local_id); state_index < partition_end;
       state_index += uint(local_size)) {
    ushort kernel_index = d_queued_kernel[state_index];
    if (kernel_index == queued_kernel) {
      uint key = d_shader_sort_key[state_index] % max_shaders;
      int index = atomic_fetch_and_add_uint32(&local_offset[key], 1);
      if (index < num_states_limit) {
        indices[index] = state_index;
      }
    }
  }
}

#endif /* __KERNEL_LOCAL_ATOMIC_SORT__ */

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
