/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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

/* TODO: abstract more device differences, define `ccl_gpu_local_syncthreads`,
 * `ccl_gpu_thread_warp`, `ccl_gpu_warp_index`, `ccl_gpu_num_warps` for all devices
 * and keep device specific code in `compat.h`. */

#ifdef __KERNEL_ONEAPI__

template<typename IsActiveOp>
void gpu_parallel_active_index_array_impl(const uint num_states,
                                          ccl_global int *ccl_restrict indices,
                                          ccl_global int *ccl_restrict num_indices,
                                          IsActiveOp is_active_op)
{
#  ifdef WITH_ONEAPI_SYCL_HOST_TASK
  int write_index = 0;
  for (int state_index = 0; state_index < num_states; state_index++) {
    if (is_active_op(state_index))
      indices[write_index++] = state_index;
  }
  *num_indices = write_index;
  return;
#  endif /* WITH_ONEAPI_SYCL_HOST_TASK */

  const sycl::nd_item<1> &item_id = sycl::ext::oneapi::this_work_item::get_nd_item<1>();
  const uint blocksize = item_id.get_local_range(0);

  sycl::multi_ptr<int[GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE + 1],
                  sycl::access::address_space::local_space>
      ptr = sycl::ext::oneapi::group_local_memory<
          int[GPU_PARALLEL_ACTIVE_INDEX_DEFAULT_BLOCK_SIZE + 1]>(item_id.get_group());
  int *warp_offset = *ptr;

  /* NOTE(@nsirgien): Here we calculate the same value as below but
   * faster for DPC++ : seems CUDA converting "%", "/", "*" based calculations below into
   * something faster already but DPC++ doesn't, so it's better to use
   * direct request of needed parameters - switching from this computation to computation below
   * will cause 2.5x performance slowdown. */
  const uint thread_index = item_id.get_local_id(0);
  const uint thread_warp = item_id.get_sub_group().get_local_id();

  const uint warp_index = item_id.get_sub_group().get_group_id();
  const uint num_warps = item_id.get_sub_group().get_group_range()[0];

  const uint state_index = item_id.get_global_id(0);

  /* Test if state corresponding to this thread is active. */
  const uint is_active = (state_index < num_states) ? is_active_op(state_index) : 0;
#else /* !__KERNEL__ONEAPI__ */
#  ifndef __KERNEL_METAL__
template<typename IsActiveOp>
__device__
#  endif
    void
    gpu_parallel_active_index_array_impl(const uint num_states,
                                         ccl_global int *indices,
                                         ccl_global int *num_indices,
#  ifdef __KERNEL_METAL__
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
#  else
                                          IsActiveOp is_active_op)
{
  extern ccl_gpu_shared int warp_offset[];

#    ifndef __KERNEL_METAL__
  const uint blocksize = ccl_gpu_block_dim_x;
#    endif

  const uint thread_index = ccl_gpu_thread_idx_x;
  const uint thread_warp = thread_index % ccl_gpu_warp_size;

  const uint warp_index = thread_index / ccl_gpu_warp_size;
  const uint num_warps = blocksize / ccl_gpu_warp_size;

  const uint state_index = ccl_gpu_block_idx_x * blocksize + thread_index;

  /* Test if state corresponding to this thread is active. */
  const uint is_active = (state_index < num_states) ? is_active_op(state_index) : 0;
#  endif
#endif /* !__KERNEL_ONEAPI__ */
  /* For each thread within a warp compute how many other active states precede it. */
#ifdef __KERNEL_ONEAPI__
  const uint thread_offset = sycl::exclusive_scan_over_group(
      item_id.get_sub_group(), is_active, std::plus<>());
#else
  const uint thread_offset = popcount(ccl_gpu_ballot(is_active) &
                                      ccl_gpu_thread_mask(thread_warp));
#endif

  /* Last thread in warp stores number of active states for each warp. */
#ifdef __KERNEL_ONEAPI__
  if (thread_warp == item_id.get_sub_group().get_local_range()[0] - 1) {
#else
  if (thread_warp == ccl_gpu_warp_size - 1) {
#endif
    warp_offset[warp_index] = thread_offset + is_active;
  }

#ifdef __KERNEL_ONEAPI__
  /* NOTE(@nsirgien): For us here only local memory writing (warp_offset) is important,
   * so faster local barriers can be used. */
  ccl_gpu_local_syncthreads();
#else
  ccl_gpu_syncthreads();
#endif

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

#ifdef __KERNEL_ONEAPI__
  /* NOTE(@nsirgien): For us here only important local memory writing (warp_offset),
   * so faster local barriers can be used. */
  ccl_gpu_local_syncthreads();
#else
  ccl_gpu_syncthreads();
#endif

  /* Write to index array. */
  if (is_active) {
    const uint block_offset = warp_offset[num_warps];
    indices[block_offset + warp_offset[warp_index] + thread_offset] = state_index;
  }
}

#ifdef __KERNEL_METAL__

#  define gpu_parallel_active_index_array(num_states, indices, num_indices, is_active_op) \
    const uint is_active = (ccl_gpu_global_id_x() < num_states) ? \
                               is_active_op(ccl_gpu_global_id_x()) : \
                               0; \
    gpu_parallel_active_index_array_impl(num_states, \
                                         indices, \
                                         num_indices, \
                                         is_active, \
                                         metal_local_size, \
                                         metal_local_id, \
                                         metal_global_id, \
                                         simdgroup_size, \
                                         simd_lane_index, \
                                         simd_group_index, \
                                         num_simd_groups, \
                                         (threadgroup int *)threadgroup_array)
#elif defined(__KERNEL_ONEAPI__)

#  define gpu_parallel_active_index_array(num_states, indices, num_indices, is_active_op) \
    gpu_parallel_active_index_array_impl(num_states, indices, num_indices, is_active_op)

#else

#  define gpu_parallel_active_index_array(num_states, indices, num_indices, is_active_op) \
    gpu_parallel_active_index_array_impl(num_states, indices, num_indices, is_active_op)

#endif

CCL_NAMESPACE_END
