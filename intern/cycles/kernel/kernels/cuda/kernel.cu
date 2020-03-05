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
#include "kernel/kernel_color.h"
#include "kernel/kernels/cuda/kernel_cuda_image.h"
#include "kernel/kernel_film.h"
#include "kernel/kernel_path.h"
#include "kernel/kernel_path_branched.h"
#include "kernel/kernel_bake.h"
#include "kernel/kernel_work_stealing.h"
#include "kernel/kernel_adaptive_sampling.h"

/* kernels */
extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_path_trace(WorkTile *tile, uint total_work_size)
{
	int work_index = ccl_global_id(0);
	bool thread_is_active = work_index < total_work_size;
	uint x, y, sample;
	KernelGlobals kg;
	if(thread_is_active) {
		get_work_pixel(tile, work_index, &x, &y, &sample);

		kernel_path_trace(&kg, tile->buffer, sample, x, y, tile->offset, tile->stride);
	}

	if(kernel_data.film.cryptomatte_passes) {
		__syncthreads();
		if(thread_is_active) {
			kernel_cryptomatte_post(&kg, tile->buffer, sample, x, y, tile->offset, tile->stride);
		}
	}
}

#ifdef __BRANCHED_PATH__
extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_BRANCHED_MAX_REGISTERS)
kernel_cuda_branched_path_trace(WorkTile *tile, uint total_work_size)
{
	int work_index = ccl_global_id(0);
	bool thread_is_active = work_index < total_work_size;
	uint x, y, sample;
	KernelGlobals kg;
	if(thread_is_active) {
		get_work_pixel(tile, work_index, &x, &y, &sample);

		kernel_branched_path_trace(&kg, tile->buffer, sample, x, y, tile->offset, tile->stride);
	}
	
	if(kernel_data.film.cryptomatte_passes) {
		__syncthreads();
		if(thread_is_active) {
			kernel_cryptomatte_post(&kg, tile->buffer, sample, x, y, tile->offset, tile->stride);
		}
	}
}
#endif

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_adaptive_stopping(WorkTile *tile, int sample, uint total_work_size)
{
	int work_index = ccl_global_id(0);
	bool thread_is_active = work_index < total_work_size;
	KernelGlobals kg;
	if(thread_is_active && kernel_data.film.pass_adaptive_aux_buffer) {
		uint x = tile->x + work_index % tile->w;
		uint y = tile->y + work_index / tile->w;
		int index = tile->offset + x + y * tile->stride;
		ccl_global float *buffer = tile->buffer + index * kernel_data.film.pass_stride;
		kernel_do_adaptive_stopping(&kg, buffer, sample);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_adaptive_filter_x(WorkTile *tile, int sample, uint)
{
	KernelGlobals kg;
	if(kernel_data.film.pass_adaptive_aux_buffer && sample > kernel_data.integrator.adaptive_min_samples) {
		if(ccl_global_id(0) < tile->h) {
			int y = tile->y + ccl_global_id(0);
			kernel_do_adaptive_filter_x(&kg, y, tile);
		}
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_adaptive_filter_y(WorkTile *tile, int sample, uint)
{
	KernelGlobals kg;
	if(kernel_data.film.pass_adaptive_aux_buffer && sample > kernel_data.integrator.adaptive_min_samples) {
		if(ccl_global_id(0) < tile->w) {
			int x = tile->x + ccl_global_id(0);
			kernel_do_adaptive_filter_y(&kg, x, tile);
		}
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_adaptive_scale_samples(WorkTile *tile, int start_sample, int sample, uint total_work_size)
{
	if(kernel_data.film.pass_adaptive_aux_buffer) {
		int work_index = ccl_global_id(0);
		bool thread_is_active = work_index < total_work_size;
		KernelGlobals kg;
		if(thread_is_active) {
			uint x = tile->x + work_index % tile->w;
			uint y = tile->y + work_index / tile->w;
			int index = tile->offset + x + y * tile->stride;
			ccl_global float *buffer = tile->buffer + index * kernel_data.film.pass_stride;
			if(buffer[kernel_data.film.pass_sample_count] < 0.0f) {
				buffer[kernel_data.film.pass_sample_count] = -buffer[kernel_data.film.pass_sample_count];
				float sample_multiplier = sample / max((float)start_sample + 1.0f, buffer[kernel_data.film.pass_sample_count]);
				if(sample_multiplier != 1.0f) {
					kernel_adaptive_post_adjust(&kg, buffer, sample_multiplier);
				}
			}
			else {
				kernel_adaptive_post_adjust(&kg, buffer, sample / (sample - 1.0f));
			}
		}
	}
}

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

