/*
 * Copyright 2011-2016 Blender Foundation
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

/* CUDA split kernel entry points */

#ifdef __CUDA_ARCH__

#define __SPLIT_KERNEL__

#include "kernel/kernel_compat_cuda.h"
#include "kernel_config.h"

#include "kernel/split/kernel_split_common.h"
#include "kernel/split/kernel_data_init.h"
#include "kernel/split/kernel_path_init.h"
#include "kernel/split/kernel_scene_intersect.h"
#include "kernel/split/kernel_lamp_emission.h"
#include "kernel/split/kernel_do_volume.h"
#include "kernel/split/kernel_queue_enqueue.h"
#include "kernel/split/kernel_indirect_background.h"
#include "kernel/split/kernel_shader_setup.h"
#include "kernel/split/kernel_shader_sort.h"
#include "kernel/split/kernel_shader_eval.h"
#include "kernel/split/kernel_holdout_emission_blurring_pathtermination_ao.h"
#include "kernel/split/kernel_subsurface_scatter.h"
#include "kernel/split/kernel_direct_lighting.h"
#include "kernel/split/kernel_shadow_blocked_ao.h"
#include "kernel/split/kernel_shadow_blocked_dl.h"
#include "kernel/split/kernel_enqueue_inactive.h"
#include "kernel/split/kernel_next_iteration_setup.h"
#include "kernel/split/kernel_indirect_subsurface.h"
#include "kernel/split/kernel_buffer_update.h"
#include "kernel/split/kernel_adaptive_stopping.h"
#include "kernel/split/kernel_adaptive_filter_x.h"
#include "kernel/split/kernel_adaptive_filter_y.h"
#include "kernel/split/kernel_adaptive_adjust_samples.h"

#include "kernel/kernel_film.h"

/* kernels */
extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_state_buffer_size(uint num_threads, uint64_t *size)
{
	*size = split_data_buffer_size(NULL, num_threads);
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_path_trace_data_init(
        ccl_global void *split_data_buffer,
        int num_elements,
        ccl_global char *ray_state,
        int start_sample,
        int end_sample,
        int sx, int sy, int sw, int sh, int offset, int stride,
        ccl_global int *Queue_index,
        int queuesize,
        ccl_global char *use_queues_flag,
        ccl_global unsigned int *work_pool_wgs,
        unsigned int num_samples,
        ccl_global float *buffer)
{
	kernel_data_init(NULL,
	                 NULL,
	                 split_data_buffer,
	                 num_elements,
	                 ray_state,
	                 start_sample,
	                 end_sample,
	                 sx, sy, sw, sh, offset, stride,
	                 Queue_index,
	                 queuesize,
	                 use_queues_flag,
	                 work_pool_wgs,
	                 num_samples,
	                 buffer);
}

#define DEFINE_SPLIT_KERNEL_FUNCTION(name) \
	extern "C" __global__ void \
	CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_SPLIT_MAX_REGISTERS) \
	kernel_cuda_##name() \
	{ \
		kernel_##name(NULL); \
	}

#define DEFINE_SPLIT_KERNEL_FUNCTION_LOCALS(name, type) \
	extern "C" __global__ void \
	CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_SPLIT_MAX_REGISTERS) \
	kernel_cuda_##name() \
	{ \
		ccl_local type locals; \
		kernel_##name(NULL, &locals); \
	}

DEFINE_SPLIT_KERNEL_FUNCTION(path_init)
DEFINE_SPLIT_KERNEL_FUNCTION(scene_intersect)
DEFINE_SPLIT_KERNEL_FUNCTION(lamp_emission)
DEFINE_SPLIT_KERNEL_FUNCTION(do_volume)
DEFINE_SPLIT_KERNEL_FUNCTION_LOCALS(queue_enqueue, QueueEnqueueLocals)
DEFINE_SPLIT_KERNEL_FUNCTION(indirect_background)
DEFINE_SPLIT_KERNEL_FUNCTION_LOCALS(shader_setup, uint)
DEFINE_SPLIT_KERNEL_FUNCTION_LOCALS(shader_sort, ShaderSortLocals)
DEFINE_SPLIT_KERNEL_FUNCTION(shader_eval)
DEFINE_SPLIT_KERNEL_FUNCTION_LOCALS(holdout_emission_blurring_pathtermination_ao, BackgroundAOLocals)
DEFINE_SPLIT_KERNEL_FUNCTION(subsurface_scatter)
DEFINE_SPLIT_KERNEL_FUNCTION_LOCALS(direct_lighting, uint)
DEFINE_SPLIT_KERNEL_FUNCTION(shadow_blocked_ao)
DEFINE_SPLIT_KERNEL_FUNCTION(shadow_blocked_dl)
DEFINE_SPLIT_KERNEL_FUNCTION_LOCALS(enqueue_inactive, uint)
DEFINE_SPLIT_KERNEL_FUNCTION_LOCALS(next_iteration_setup, uint)
DEFINE_SPLIT_KERNEL_FUNCTION(indirect_subsurface)
DEFINE_SPLIT_KERNEL_FUNCTION_LOCALS(buffer_update, uint)
DEFINE_SPLIT_KERNEL_FUNCTION(adaptive_stopping)
DEFINE_SPLIT_KERNEL_FUNCTION(adaptive_filter_x)
DEFINE_SPLIT_KERNEL_FUNCTION(adaptive_filter_y)
DEFINE_SPLIT_KERNEL_FUNCTION(adaptive_adjust_samples)

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

#endif

