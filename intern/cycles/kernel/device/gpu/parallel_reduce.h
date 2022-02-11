/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

#pragma once

CCL_NAMESPACE_BEGIN

/* Parallel sum of array input_data with size n into output_sum.
 *
 * Adapted from "Optimizing Parallel Reduction in GPU", Mark Harris.
 *
 * This version adds multiple elements per thread sequentially.  This reduces
 * the overall cost of the algorithm while keeping the work complexity O(n) and
 * the step complexity O(log n). (Brent's Theorem optimization) */

#ifdef __HIP__
#  define GPU_PARALLEL_SUM_DEFAULT_BLOCK_SIZE 1024
#else
#  define GPU_PARALLEL_SUM_DEFAULT_BLOCK_SIZE 512
#endif

template<uint blocksize, typename InputT, typename OutputT, typename ConvertOp>
__device__ void gpu_parallel_sum(
    const InputT *input_data, const uint n, OutputT *output_sum, OutputT zero, ConvertOp convert)
{
  extern ccl_gpu_shared OutputT shared_data[];

  const uint tid = ccl_gpu_thread_idx_x;
  const uint gridsize = blocksize * ccl_gpu_grid_dim_x();

  OutputT sum = zero;
  for (uint i = ccl_gpu_block_idx_x * blocksize + tid; i < n; i += gridsize) {
    sum += convert(input_data[i]);
  }
  shared_data[tid] = sum;

  ccl_gpu_syncthreads();

  if (blocksize >= 512 && tid < 256) {
    shared_data[tid] = sum = sum + shared_data[tid + 256];
  }

  ccl_gpu_syncthreads();

  if (blocksize >= 256 && tid < 128) {
    shared_data[tid] = sum = sum + shared_data[tid + 128];
  }

  ccl_gpu_syncthreads();

  if (blocksize >= 128 && tid < 64) {
    shared_data[tid] = sum = sum + shared_data[tid + 64];
  }

  ccl_gpu_syncthreads();

  if (blocksize >= 64 && tid < 32) {
    shared_data[tid] = sum = sum + shared_data[tid + 32];
  }

  ccl_gpu_syncthreads();

  if (tid < 32) {
    for (int offset = ccl_gpu_warp_size / 2; offset > 0; offset /= 2) {
      sum += ccl_shfl_down_sync(0xFFFFFFFF, sum, offset);
    }
  }

  if (tid == 0) {
    output_sum[ccl_gpu_block_idx_x] = sum;
  }
}

CCL_NAMESPACE_END
