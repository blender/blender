/*
 * Copyright 2011-2013 Blender Foundation
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

/* device data taken from CUDA occupancy calculator */

/* 3.0 and 3.5 */
#if __CUDA_ARCH__ == 300 || __CUDA_ARCH__ == 350
#  define CUDA_MULTIPRESSOR_MAX_REGISTERS 65536
#  define CUDA_MULTIPROCESSOR_MAX_BLOCKS 16
#  define CUDA_BLOCK_MAX_THREADS 1024
#  define CUDA_THREAD_MAX_REGISTERS 63

/* tunable parameters */
#  define CUDA_THREADS_BLOCK_WIDTH 16
#  define CUDA_KERNEL_MAX_REGISTERS 63
#  define CUDA_KERNEL_BRANCHED_MAX_REGISTERS 63

/* 3.2 */
#elif __CUDA_ARCH__ == 320
#  define CUDA_MULTIPRESSOR_MAX_REGISTERS 32768
#  define CUDA_MULTIPROCESSOR_MAX_BLOCKS 16
#  define CUDA_BLOCK_MAX_THREADS 1024
#  define CUDA_THREAD_MAX_REGISTERS 63

/* tunable parameters */
#  define CUDA_THREADS_BLOCK_WIDTH 16
#  define CUDA_KERNEL_MAX_REGISTERS 63
#  define CUDA_KERNEL_BRANCHED_MAX_REGISTERS 63

/* 3.7 */
#elif __CUDA_ARCH__ == 370
#  define CUDA_MULTIPRESSOR_MAX_REGISTERS 65536
#  define CUDA_MULTIPROCESSOR_MAX_BLOCKS 16
#  define CUDA_BLOCK_MAX_THREADS 1024
#  define CUDA_THREAD_MAX_REGISTERS 255

/* tunable parameters */
#  define CUDA_THREADS_BLOCK_WIDTH 16
#  define CUDA_KERNEL_MAX_REGISTERS 63
#  define CUDA_KERNEL_BRANCHED_MAX_REGISTERS 63

/* 5.0, 5.2, 5.3, 6.0, 6.1 */
#elif __CUDA_ARCH__ >= 500
#  define CUDA_MULTIPRESSOR_MAX_REGISTERS 65536
#  define CUDA_MULTIPROCESSOR_MAX_BLOCKS 32
#  define CUDA_BLOCK_MAX_THREADS 1024
#  define CUDA_THREAD_MAX_REGISTERS 255

/* tunable parameters */
#  define CUDA_THREADS_BLOCK_WIDTH 16
/* CUDA 9.0 seems to cause slowdowns on high-end Pascal cards unless we increase the number of registers */
#  if __CUDACC_VER_MAJOR__ == 9 && __CUDA_ARCH__ >= 600
#    define CUDA_KERNEL_MAX_REGISTERS 64
#  else
#    define CUDA_KERNEL_MAX_REGISTERS 48
#  endif
#  define CUDA_KERNEL_BRANCHED_MAX_REGISTERS 63


/* unknown architecture */
#else
#  error "Unknown or unsupported CUDA architecture, can't determine launch bounds"
#endif

/* For split kernel using all registers seems fastest for now, but this
 * is unlikely to be optimal once we resolve other bottlenecks. */

#define CUDA_KERNEL_SPLIT_MAX_REGISTERS CUDA_THREAD_MAX_REGISTERS

/* Compute number of threads per block and minimum blocks per multiprocessor
 * given the maximum number of registers per thread. */

#define CUDA_LAUNCH_BOUNDS(threads_block_width, thread_num_registers) \
	__launch_bounds__( \
		threads_block_width*threads_block_width, \
		CUDA_MULTIPRESSOR_MAX_REGISTERS/(threads_block_width*threads_block_width*thread_num_registers) \
		)

/* sanity checks */

#if CUDA_THREADS_BLOCK_WIDTH*CUDA_THREADS_BLOCK_WIDTH > CUDA_BLOCK_MAX_THREADS
#  error "Maximum number of threads per block exceeded"
#endif

#if CUDA_MULTIPRESSOR_MAX_REGISTERS/(CUDA_THREADS_BLOCK_WIDTH*CUDA_THREADS_BLOCK_WIDTH*CUDA_KERNEL_MAX_REGISTERS) > CUDA_MULTIPROCESSOR_MAX_BLOCKS
#  error "Maximum number of blocks per multiprocessor exceeded"
#endif

#if CUDA_KERNEL_MAX_REGISTERS > CUDA_THREAD_MAX_REGISTERS
#  error "Maximum number of registers per thread exceeded"
#endif

#if CUDA_KERNEL_BRANCHED_MAX_REGISTERS > CUDA_THREAD_MAX_REGISTERS
#  error "Maximum number of registers per thread exceeded"
#endif
