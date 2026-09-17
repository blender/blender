/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Given an array of states, build an array of indices for which the states
 * are active and sorted by a given key. The prefix sum of the number of active
 * states per key must have already been computed.
 *
 * TODO: there may be ways to optimize this to avoid this many atomic ops? */

#include "kernel/device/gpu/block_sizes.h"
#include "util/atomic.h"

ccl_device_inline void gpu_parallel_sort_bucket_pass_impl(const uint num_states,
                                                          const uint partition_size,
                                                          const uint max_shaders,
                                                          const uint queued_kernel,
#if defined(__KERNEL_METAL__) || defined(__KERNEL_ONEAPI__)
                                                          ccl_gpu_shared int *buckets,
#endif
#ifdef __KERNEL_METAL__
                                                          const ushort local_id,
                                                          const ushort local_size,
                                                          const uint grid_id,
#endif
                                                          ccl_global ushort *d_queued_kernel,
                                                          ccl_global uint *d_shader_sort_key,
                                                          ccl_global int *partition_key_offsets)
{
#ifndef __KERNEL_METAL__
#  ifndef __KERNEL_ONEAPI__
  extern ccl_gpu_shared int buckets[];
#  endif

  const ushort local_id = ccl_gpu_thread_idx_x;
  const ushort local_size = ccl_gpu_block_dim_x;
  const uint grid_id = ccl_gpu_block_idx_x;
#endif

  /* Zero the bucket sizes. */
  for (uint i = local_id; i < max_shaders; i += local_size) {
    atomic_store_local(&buckets[i], 0);
  }

#ifdef __KERNEL_ONEAPI__
  /* NOTE(@nsirgien): For us here only local memory writing (buckets) is important,
   * so faster local barriers can be used. */
  ccl_gpu_local_syncthreads();
#else
  ccl_gpu_syncthreads();
#endif

  /* Determine bucket sizes within the partitions. */

  const uint partition_start = partition_size * uint(grid_id);
  const uint partition_end = min(num_states, partition_start + partition_size);

  for (int state_index = partition_start + uint(local_id); state_index < partition_end;
       state_index += uint(local_size))
  {
    ushort kernel_index = d_queued_kernel[state_index];
    if (kernel_index == queued_kernel) {
      uint key = d_shader_sort_key[state_index] % max_shaders;
      atomic_fetch_and_add_uint32_shared(&buckets[key], 1);
    }
  }

#ifdef __KERNEL_ONEAPI__
  /* NOTE(@nsirgien): For us here only local memory writing (buckets) is important,
   * so faster local barriers can be used. */
  ccl_gpu_local_syncthreads();
#else
  ccl_gpu_syncthreads();
#endif

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

#ifdef __KERNEL_METAL__

#  define gpu_parallel_sort_bucket_pass(num_states, \
                                        partition_size, \
                                        max_shaders, \
                                        queued_kernel, \
                                        d_queued_kernel, \
                                        d_shader_sort_key, \
                                        partition_key_offsets) \
    gpu_parallel_sort_bucket_pass_impl(num_states, \
                                       partition_size, \
                                       max_shaders, \
                                       queued_kernel, \
                                       (ccl_gpu_shared int *)threadgroup_array, \
                                       metal_local_id, \
                                       metal_local_size, \
                                       metal_grid_id, \
                                       d_queued_kernel, \
                                       d_shader_sort_key, \
                                       partition_key_offsets)

#elif defined(__KERNEL_ONEAPI__)

#  define gpu_parallel_sort_bucket_pass(num_states, \
                                        partition_size, \
                                        max_shaders, \
                                        queued_kernel, \
                                        d_queued_kernel, \
                                        d_shader_sort_key, \
                                        partition_key_offsets) \
    gpu_parallel_sort_bucket_pass_impl(num_states, \
                                       partition_size, \
                                       max_shaders, \
                                       queued_kernel, \
                                       (ccl_gpu_shared int *)threadgroup_array, \
                                       d_queued_kernel, \
                                       d_shader_sort_key, \
                                       partition_key_offsets)

#else

#  define gpu_parallel_sort_bucket_pass gpu_parallel_sort_bucket_pass_impl

#endif

ccl_device_inline void gpu_parallel_sort_write_pass_impl(const uint num_states,
                                                         const uint partition_size,
                                                         const int num_states_limit,
                                                         ccl_global int *indices,
                                                         const uint max_shaders,
                                                         const uint queued_kernel,
#if defined(__KERNEL_METAL__) || defined(__KERNEL_ONEAPI__)
                                                         ccl_gpu_shared int *local_offset,
#endif
#ifdef __KERNEL_METAL__
                                                         const ushort local_id,
                                                         const ushort local_size,
                                                         const uint grid_id,
#endif
                                                         ccl_global ushort *d_queued_kernel,
                                                         ccl_global uint *d_shader_sort_key,
                                                         ccl_global int *partition_key_offsets)
{
#ifndef __KERNEL_METAL__
#  ifndef __KERNEL_ONEAPI__
  extern ccl_gpu_shared int local_offset[];
#  endif

  const ushort local_id = ccl_gpu_thread_idx_x;
  const ushort local_size = ccl_gpu_block_dim_x;
  const uint grid_id = ccl_gpu_block_idx_x;
#endif

  /* Calculate each partition's global offset from the prefix sum of the active state counts per
   * partition. */

  int partition_offset = 0;
  for (uint i = 0; i < grid_id; i++) {
    partition_offset += partition_key_offsets[max_shaders + i * (max_shaders + 1)];
  }

  ccl_global int *key_offsets = partition_key_offsets + grid_id * (max_shaders + 1);
  for (uint i = local_id; i < max_shaders; i += local_size) {
    atomic_store_local(&local_offset[i], key_offsets[i] + partition_offset);
  }

#ifdef __KERNEL_ONEAPI__
  /* NOTE(@nsirgien): For us here only local memory writing (local_offset) is important,
   * so faster local barriers can be used. */
  ccl_gpu_local_syncthreads();
#else
  ccl_gpu_syncthreads();
#endif

  /* Write the sorted active indices. */

  const uint partition_start = partition_size * uint(grid_id);
  const uint partition_end = min(num_states, partition_start + partition_size);

  for (int state_index = partition_start + uint(local_id); state_index < partition_end;
       state_index += uint(local_size))
  {
    ushort kernel_index = d_queued_kernel[state_index];
    if (kernel_index == queued_kernel) {
      uint key = d_shader_sort_key[state_index] % max_shaders;
      int index = atomic_fetch_and_add_uint32_shared(&local_offset[key], 1);
      if (index < num_states_limit) {
        indices[index] = state_index;
      }
    }
  }
}

#ifdef __KERNEL_METAL__

#  define gpu_parallel_sort_write_pass(num_states, \
                                       partition_size, \
                                       num_states_limit, \
                                       indices, \
                                       max_shaders, \
                                       queued_kernel, \
                                       d_queued_kernel, \
                                       d_shader_sort_key, \
                                       partition_key_offsets) \
    gpu_parallel_sort_write_pass_impl(num_states, \
                                      partition_size, \
                                      num_states_limit, \
                                      indices, \
                                      max_shaders, \
                                      queued_kernel, \
                                      (ccl_gpu_shared int *)threadgroup_array, \
                                      metal_local_id, \
                                      metal_local_size, \
                                      metal_grid_id, \
                                      d_queued_kernel, \
                                      d_shader_sort_key, \
                                      partition_key_offsets)

#elif defined(__KERNEL_ONEAPI__)

#  define gpu_parallel_sort_write_pass(num_states, \
                                       partition_size, \
                                       num_states_limit, \
                                       indices, \
                                       max_shaders, \
                                       queued_kernel, \
                                       d_queued_kernel, \
                                       d_shader_sort_key, \
                                       partition_key_offsets) \
    gpu_parallel_sort_write_pass_impl(num_states, \
                                      partition_size, \
                                      num_states_limit, \
                                      indices, \
                                      max_shaders, \
                                      queued_kernel, \
                                      (ccl_gpu_shared int *)threadgroup_array, \
                                      d_queued_kernel, \
                                      d_shader_sort_key, \
                                      partition_key_offsets)

#else

#  define gpu_parallel_sort_write_pass gpu_parallel_sort_write_pass_impl

#endif

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
      /* Can't process this state now, increase the counter again
       * so that it will be handled in another iteration. */
      atomic_fetch_and_add_uint32(&key_counter[key], 1);
    }
  }
}

CCL_NAMESPACE_END
