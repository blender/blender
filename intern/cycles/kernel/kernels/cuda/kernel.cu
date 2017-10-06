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

/* CUDA kernel entry points */

#ifdef __CUDA_ARCH__

#include "kernel/kernel_compat_cuda.h"
#include "kernel_config.h"

#include "util/util_atomic.h"

#include "kernel/kernel_math.h"
#include "kernel/kernel_types.h"
#include "kernel/kernel_globals.h"
#include "kernel/kernels/cuda/kernel_cuda_image.h"
#include "kernel/kernel_film.h"
#include "kernel/kernel_path.h"
#include "kernel/kernel_path_branched.h"
#include "kernel/kernel_bake.h"
#include "kernel/kernel_work_stealing.h"

/* kernels */
extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_path_trace(WorkTile *tile, uint total_work_size)
{
	int work_index = ccl_global_id(0);

	if(work_index < total_work_size) {
		uint x, y, sample;
		get_work_pixel(tile, work_index, &x, &y, &sample);

		KernelGlobals kg;
		kernel_path_trace(&kg, tile->buffer, sample, x, y, tile->offset, tile->stride);
	}
}

#ifdef __BRANCHED_PATH__
extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_BRANCHED_MAX_REGISTERS)
kernel_cuda_branched_path_trace(WorkTile *tile, uint total_work_size)
{
	int work_index = ccl_global_id(0);

	if(work_index < total_work_size) {
		uint x, y, sample;
		get_work_pixel(tile, work_index, &x, &y, &sample);

		KernelGlobals kg;
		kernel_branched_path_trace(&kg, tile->buffer, sample, x, y, tile->offset, tile->stride);
	}
}
#endif

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_convert_to_byte(uchar4 *rgba, float *buffer, float sample_scale, int sx, int sy, int sw, int sh, int offset, int stride)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;
	int y = sy + blockDim.y*blockIdx.y + threadIdx.y;

	if(x < sx + sw && y < sy + sh) {
		kernel_film_convert_to_byte(NULL, rgba, buffer, sample_scale, x, y, offset, stride);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_convert_to_half_float(uchar4 *rgba, float *buffer, float sample_scale, int sx, int sy, int sw, int sh, int offset, int stride)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;
	int y = sy + blockDim.y*blockIdx.y + threadIdx.y;

	if(x < sx + sw && y < sy + sh) {
		kernel_film_convert_to_half_float(NULL, rgba, buffer, sample_scale, x, y, offset, stride);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_displace(uint4 *input,
                     float4 *output,
                     int type,
                     int sx,
                     int sw,
                     int offset,
                     int sample)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;

	if(x < sx + sw) {
		KernelGlobals kg;
		kernel_displace_evaluate(&kg, input, output, x);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_background(uint4 *input,
                       float4 *output,
                       int type,
                       int sx,
                       int sw,
                       int offset,
                       int sample)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;

	if(x < sx + sw) {
		KernelGlobals kg;
		kernel_background_evaluate(&kg, input, output, x);
	}
}

#ifdef __BAKING__
extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_bake(uint4 *input, float4 *output, int type, int filter, int sx, int sw, int offset, int sample)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;

	if(x < sx + sw) {
		KernelGlobals kg;
		kernel_bake_evaluate(&kg, input, output, (ShaderEvalType)type, filter, x, offset, sample);
	}
}
#endif

#endif

