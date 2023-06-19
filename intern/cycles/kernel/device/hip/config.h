/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Device data taken from HIP occupancy calculator.
 *
 * Terminology
 * - HIP GPUs have multiple streaming multiprocessors
 * - Each multiprocessor executes multiple thread blocks
 * - Each thread block contains a number of threads, also known as the block size
 * - Multiprocessors have a fixed number of registers, and the amount of registers
 *   used by each threads limits the number of threads per block.
 */

/* Launch Bound Definitions */
#define GPU_MULTIPRESSOR_MAX_REGISTERS 65536
#define GPU_MULTIPROCESSOR_MAX_BLOCKS 64
#define GPU_BLOCK_MAX_THREADS 1024
#define GPU_THREAD_MAX_REGISTERS 255

#define GPU_KERNEL_BLOCK_NUM_THREADS 1024
#define GPU_KERNEL_MAX_REGISTERS 64

/* For performance tuning of HIPRT kernels we might have to change the number
 * that's why we don't use GPU_KERNEL_BLOCK_NUM_THREADS. */
#define GPU_HIPRT_KERNEL_BLOCK_NUM_THREADS 1024

/* Compute number of threads per block and minimum blocks per multiprocessor
 * given the maximum number of registers per thread. */
#define ccl_gpu_kernel(block_num_threads, thread_num_registers) \
  extern "C" __global__ void __launch_bounds__(block_num_threads, \
                                               GPU_MULTIPRESSOR_MAX_REGISTERS / \
                                                   (block_num_threads * thread_num_registers))

#define ccl_gpu_kernel_threads(block_num_threads) \
  extern "C" __global__ void __launch_bounds__(block_num_threads)

#define ccl_gpu_kernel_signature(name, ...) kernel_gpu_##name(__VA_ARGS__)
#define ccl_gpu_kernel_postfix

#define ccl_gpu_kernel_call(x) x
#define ccl_gpu_kernel_within_bounds(i, n) ((i) < (n))

/* Define a function object where "func" is the lambda body, and additional parameters are used to
 * specify captured state  */
#define ccl_gpu_kernel_lambda(func, ...) \
  struct KernelLambda { \
    __VA_ARGS__; \
    __device__ int operator()(const int state) \
    { \
      return (func); \
    } \
  } ccl_gpu_kernel_lambda_pass

/* sanity checks */

#if GPU_KERNEL_BLOCK_NUM_THREADS > GPU_BLOCK_MAX_THREADS
#  error "Maximum number of threads per block exceeded"
#endif

#if GPU_MULTIPRESSOR_MAX_REGISTERS / (GPU_KERNEL_BLOCK_NUM_THREADS * GPU_KERNEL_MAX_REGISTERS) > \
    GPU_MULTIPROCESSOR_MAX_BLOCKS
#  error "Maximum number of blocks per multiprocessor exceeded"
#endif

#if GPU_KERNEL_MAX_REGISTERS > GPU_THREAD_MAX_REGISTERS
#  error "Maximum number of registers per thread exceeded"
#endif
