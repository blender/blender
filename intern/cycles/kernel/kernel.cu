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
 * limitations under the License
 */

/* CUDA kernel entry points */

#include "kernel_compat_cuda.h"
#include "kernel_math.h"
#include "kernel_types.h"
#include "kernel_globals.h"
#include "kernel_film.h"
#include "kernel_path.h"
#include "kernel_bake.h"

/* device data taken from CUDA occupancy calculator */

#ifdef __CUDA_ARCH__

/* 2.0 and 2.1 */
#if __CUDA_ARCH__ == 200 || __CUDA_ARCH__ == 210
#define CUDA_MULTIPRESSOR_MAX_REGISTERS 32768
#define CUDA_MULTIPROCESSOR_MAX_BLOCKS 8
#define CUDA_BLOCK_MAX_THREADS 1024
#define CUDA_THREAD_MAX_REGISTERS 63

/* tunable parameters */
#define CUDA_THREADS_BLOCK_WIDTH 16
#define CUDA_KERNEL_MAX_REGISTERS 32
#define CUDA_KERNEL_BRANCHED_MAX_REGISTERS 40

/* 3.0 and 3.5 */
#elif __CUDA_ARCH__ == 300 || __CUDA_ARCH__ == 350
#define CUDA_MULTIPRESSOR_MAX_REGISTERS 65536
#define CUDA_MULTIPROCESSOR_MAX_BLOCKS 16
#define CUDA_BLOCK_MAX_THREADS 1024
#define CUDA_THREAD_MAX_REGISTERS 63

/* tunable parameters */
#define CUDA_THREADS_BLOCK_WIDTH 16
#define CUDA_KERNEL_MAX_REGISTERS 63
#define CUDA_KERNEL_BRANCHED_MAX_REGISTERS 63

/* 5.0 and 5.2 */
#elif __CUDA_ARCH__ == 500 || __CUDA_ARCH__ == 520
#define CUDA_MULTIPRESSOR_MAX_REGISTERS 65536
#define CUDA_MULTIPROCESSOR_MAX_BLOCKS 32
#define CUDA_BLOCK_MAX_THREADS 1024
#define CUDA_THREAD_MAX_REGISTERS 255

/* tunable parameters */
#define CUDA_THREADS_BLOCK_WIDTH 16
#define CUDA_KERNEL_MAX_REGISTERS 40
#define CUDA_KERNEL_BRANCHED_MAX_REGISTERS 63

/* unknown architecture */
#else
#error "Unknown or unsupported CUDA architecture, can't determine launch bounds"
#endif

/* compute number of threads per block and minimum blocks per multiprocessor
 * given the maximum number of registers per thread */

#define CUDA_LAUNCH_BOUNDS(threads_block_width, thread_num_registers) \
	__launch_bounds__( \
		threads_block_width*threads_block_width, \
		CUDA_MULTIPRESSOR_MAX_REGISTERS/(threads_block_width*threads_block_width*thread_num_registers) \
		)

/* sanity checks */

#if CUDA_THREADS_BLOCK_WIDTH*CUDA_THREADS_BLOCK_WIDTH > CUDA_BLOCK_MAX_THREADS
#error "Maximum number of threads per block exceeded"
#endif

#if CUDA_MULTIPRESSOR_MAX_REGISTERS/(CUDA_THREADS_BLOCK_WIDTH*CUDA_THREADS_BLOCK_WIDTH*CUDA_KERNEL_MAX_REGISTERS) > CUDA_MULTIPROCESSOR_MAX_BLOCKS
#error "Maximum number of blocks per multiprocessor exceeded"
#endif

#if CUDA_KERNEL_MAX_REGISTERS > CUDA_THREAD_MAX_REGISTERS
#error "Maximum number of registers per thread exceeded"
#endif

#if CUDA_KERNEL_BRANCHED_MAX_REGISTERS > CUDA_THREAD_MAX_REGISTERS
#error "Maximum number of registers per thread exceeded"
#endif

/* kernels */

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_path_trace(float *buffer, uint *rng_state, int sample, int sx, int sy, int sw, int sh, int offset, int stride)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;
	int y = sy + blockDim.y*blockIdx.y + threadIdx.y;

	if(x < sx + sw && y < sy + sh)
		kernel_path_trace(NULL, buffer, rng_state, sample, x, y, offset, stride);
}

#ifdef __BRANCHED_PATH__
extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_BRANCHED_MAX_REGISTERS)
kernel_cuda_branched_path_trace(float *buffer, uint *rng_state, int sample, int sx, int sy, int sw, int sh, int offset, int stride)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;
	int y = sy + blockDim.y*blockIdx.y + threadIdx.y;

	if(x < sx + sw && y < sy + sh)
		kernel_branched_path_trace(NULL, buffer, rng_state, sample, x, y, offset, stride);
}
#endif

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_convert_to_byte(uchar4 *rgba, float *buffer, float sample_scale, int sx, int sy, int sw, int sh, int offset, int stride)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;
	int y = sy + blockDim.y*blockIdx.y + threadIdx.y;

	if(x < sx + sw && y < sy + sh)
		kernel_film_convert_to_byte(NULL, rgba, buffer, sample_scale, x, y, offset, stride);
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_convert_to_half_float(uchar4 *rgba, float *buffer, float sample_scale, int sx, int sy, int sw, int sh, int offset, int stride)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;
	int y = sy + blockDim.y*blockIdx.y + threadIdx.y;

	if(x < sx + sw && y < sy + sh)
		kernel_film_convert_to_half_float(NULL, rgba, buffer, sample_scale, x, y, offset, stride);
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_shader(uint4 *input, float4 *output, int type, int sx, int sw, int offset, int sample)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;

	if(x < sx + sw)
		kernel_shader_evaluate(NULL, input, output, (ShaderEvalType)type, x, sample);
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_bake(uint4 *input, float4 *output, int type, int sx, int sw, int offset, int sample)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;

	if(x < sx + sw)
		kernel_bake_evaluate(NULL, input, output, (ShaderEvalType)type, x, offset, sample);
}

#endif

