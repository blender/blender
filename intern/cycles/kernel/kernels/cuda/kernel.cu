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
#include "kernel/kernel_math.h"
#include "kernel/kernel_types.h"
#include "kernel/kernel_globals.h"
#include "kernel/kernel_film.h"
#include "kernel/kernel_path.h"
#include "kernel/kernel_path_branched.h"
#include "kernel/kernel_bake.h"

/* kernels */
extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_path_trace(float *buffer, uint *rng_state, int sample, int sx, int sy, int sw, int sh, int offset, int stride)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;
	int y = sy + blockDim.y*blockIdx.y + threadIdx.y;

	if(x < sx + sw && y < sy + sh) {
		KernelGlobals kg;
		kernel_path_trace(&kg, buffer, rng_state, sample, x, y, offset, stride);
	}
}

#ifdef __BRANCHED_PATH__
extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_BRANCHED_MAX_REGISTERS)
kernel_cuda_branched_path_trace(float *buffer, uint *rng_state, int sample, int sx, int sy, int sw, int sh, int offset, int stride)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;
	int y = sy + blockDim.y*blockIdx.y + threadIdx.y;

	if(x < sx + sw && y < sy + sh) {
		KernelGlobals kg;
		kernel_branched_path_trace(&kg, buffer, rng_state, sample, x, y, offset, stride);
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
kernel_cuda_shader(uint4 *input,
                   float4 *output,
                   float *output_luma,
                   int type,
                   int sx,
                   int sw,
                   int offset,
                   int sample)
{
	int x = sx + blockDim.x*blockIdx.x + threadIdx.x;

	if(x < sx + sw) {
		KernelGlobals kg;
		kernel_shader_evaluate(&kg,
		                       input,
		                       output,
		                       output_luma,
		                       (ShaderEvalType)type, 
		                       x,
		                       sample);
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

