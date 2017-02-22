/*
 * Copyright 2011-2015 Blender Foundation
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

CCL_NAMESPACE_BEGIN

/* Note on kernel_data_initialization kernel
 * This kernel Initializes structures needed in path-iteration kernels.
 * This is the first kernel in ray-tracing logic.
 *
 * Ray state of rays outside the tile-boundary will be marked RAY_INACTIVE
 *
 * Its input and output are as follows,
 *
 * Un-initialized rng---------------|--- kernel_data_initialization ---|--- Initialized rng
 * Un-initialized throughput -------|                                  |--- Initialized throughput
 * Un-initialized L_transparent ----|                                  |--- Initialized L_transparent
 * Un-initialized PathRadiance -----|                                  |--- Initialized PathRadiance
 * Un-initialized Ray --------------|                                  |--- Initialized Ray
 * Un-initialized PathState --------|                                  |--- Initialized PathState
 * Un-initialized QueueData --------|                                  |--- Initialized QueueData (to QUEUE_EMPTY_SLOT)
 * Un-initialized QueueIndex -------|                                  |--- Initialized QueueIndex (to 0)
 * Un-initialized use_queues_flag---|                                  |--- Initialized use_queues_flag (to false)
 * Un-initialized ray_state --------|                                  |--- Initialized ray_state
 * parallel_samples --------------- |                                  |--- Initialized per_sample_output_buffers
 * rng_state -----------------------|                                  |--- Initialized work_array
 * data ----------------------------|                                  |--- Initialized work_pool_wgs
 * start_sample --------------------|                                  |
 * sx ------------------------------|                                  |
 * sy ------------------------------|                                  |
 * sw ------------------------------|                                  |
 * sh ------------------------------|                                  |
 * stride --------------------------|                                  |
 * queuesize -----------------------|                                  |
 * num_samples ---------------------|                                  |
 *
 * Note on Queues :
 * All slots in queues are initialized to queue empty slot;
 * The number of elements in the queues is initialized to 0;
 */

ccl_device void kernel_data_init(
        KernelGlobals *kg,
        ccl_constant KernelData *data,
        ccl_global void *split_data_buffer,
        int num_elements,
        ccl_global char *ray_state,
        ccl_global uint *rng_state,

#ifdef __KERNEL_OPENCL__
#define KERNEL_TEX(type, ttype, name)                                   \
        ccl_global type *name,
#include "../kernel_textures.h"
#endif

        int start_sample,
        int end_sample,
        int sx, int sy, int sw, int sh, int offset, int stride,
        ccl_global int *Queue_index,                 /* Tracks the number of elements in queues */
        int queuesize,                               /* size (capacity) of the queue */
        ccl_global char *use_queues_flag,            /* flag to decide if scene-intersect kernel should use queues to fetch ray index */
        ccl_global unsigned int *work_pools,      /* Work pool for each work group */
        unsigned int num_samples,
        ccl_global float *buffer)
{
#ifdef __KERNEL_OPENCL__
	kg->data = data;
#endif

	kernel_split_params.x = sx;
	kernel_split_params.y = sy;
	kernel_split_params.w = sw;
	kernel_split_params.h = sh;

	kernel_split_params.offset = offset;
	kernel_split_params.stride = stride;

	kernel_split_params.rng_state = rng_state;

	kernel_split_params.start_sample = start_sample;
	kernel_split_params.end_sample = end_sample;

	kernel_split_params.work_pools = work_pools;
	kernel_split_params.num_samples = num_samples;

	kernel_split_params.queue_index = Queue_index;
	kernel_split_params.queue_size = queuesize;
	kernel_split_params.use_queues_flag = use_queues_flag;

	kernel_split_params.buffer = buffer;

	split_data_init(&kernel_split_state, num_elements, split_data_buffer, ray_state);

#ifdef __KERNEL_OPENCL__
#define KERNEL_TEX(type, ttype, name) \
	kg->name = name;
#include "../kernel_textures.h"
#endif

	int thread_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);

	/* Initialize queue data and queue index. */
	if(thread_index < queuesize) {
		/* Initialize active ray queue. */
		kernel_split_state.queue_data[QUEUE_ACTIVE_AND_REGENERATED_RAYS * queuesize + thread_index] = QUEUE_EMPTY_SLOT;
		/* Initialize background and buffer update queue. */
		kernel_split_state.queue_data[QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS * queuesize + thread_index] = QUEUE_EMPTY_SLOT;
		/* Initialize shadow ray cast of AO queue. */
		kernel_split_state.queue_data[QUEUE_SHADOW_RAY_CAST_AO_RAYS * queuesize + thread_index] = QUEUE_EMPTY_SLOT;
		/* Initialize shadow ray cast of direct lighting queue. */
		kernel_split_state.queue_data[QUEUE_SHADOW_RAY_CAST_DL_RAYS * queuesize + thread_index] = QUEUE_EMPTY_SLOT;
	}

	if(thread_index == 0) {
		Queue_index[QUEUE_ACTIVE_AND_REGENERATED_RAYS] = 0;
		Queue_index[QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS] = 0;
		Queue_index[QUEUE_SHADOW_RAY_CAST_AO_RAYS] = 0;
		Queue_index[QUEUE_SHADOW_RAY_CAST_DL_RAYS] = 0;
		/* The scene-intersect kernel should not use the queues very first time.
		 * since the queue would be empty.
		 */
		*use_queues_flag = 0;
	}

	int ray_index = ccl_global_id(0) + ccl_global_id(1) * ccl_global_size(0);

	/* This is the first assignment to ray_state;
	 * So we dont use ASSIGN_RAY_STATE macro.
	 */
	kernel_split_state.ray_state[ray_index] = RAY_ACTIVE;

	unsigned int my_sample;
	unsigned int pixel_x;
	unsigned int pixel_y;
	unsigned int tile_x;
	unsigned int tile_y;
	unsigned int my_sample_tile;

	unsigned int work_index = 0;
	/* Get work. */
	if(!get_next_work(kg, &work_index, ray_index)) {
		/* No more work, mark ray as inactive */
		kernel_split_state.ray_state[ray_index] = RAY_INACTIVE;

		return;
	}

	/* Get the sample associated with the work. */
	my_sample = get_work_sample(kg, work_index, ray_index) + start_sample;

	my_sample_tile = 0;

	/* Get pixel and tile position associated with the work. */
	get_work_pixel_tile_position(kg, &pixel_x, &pixel_y,
	                        &tile_x, &tile_y,
	                        work_index,
	                        ray_index);
	kernel_split_state.work_array[ray_index] = work_index;

	rng_state += kernel_split_params.offset + pixel_x + pixel_y*stride;

	ccl_global float *per_sample_output_buffers = kernel_split_state.per_sample_output_buffers;
	per_sample_output_buffers += ((tile_x + (tile_y * stride)) + (my_sample_tile)) * kernel_data.film.pass_stride;

	/* Initialize random numbers and ray. */
	kernel_path_trace_setup(kg,
	                        rng_state,
	                        my_sample,
	                        pixel_x, pixel_y,
	                        &kernel_split_state.rng[ray_index],
	                        &kernel_split_state.ray[ray_index]);

	if(kernel_split_state.ray[ray_index].t != 0.0f) {
		/* Initialize throughput, L_transparent, Ray, PathState;
		 * These rays proceed with path-iteration.
		 */
		kernel_split_state.throughput[ray_index] = make_float3(1.0f, 1.0f, 1.0f);
		kernel_split_state.L_transparent[ray_index] = 0.0f;
		path_radiance_init(&kernel_split_state.path_radiance[ray_index], kernel_data.film.use_light_pass);
		path_state_init(kg,
		                kernel_split_state.sd_DL_shadow,
		                &kernel_split_state.path_state[ray_index],
		                &kernel_split_state.rng[ray_index],
		                my_sample,
		                &kernel_split_state.ray[ray_index]);
#ifdef __KERNEL_DEBUG__
		debug_data_init(&kernel_split_state.debug_data[ray_index]);
#endif
	}
	else {
		/* These rays do not participate in path-iteration. */
		float4 L_rad = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
		/* Accumulate result in output buffer. */
		kernel_write_pass_float4(per_sample_output_buffers, my_sample, L_rad);
		path_rng_end(kg, rng_state, kernel_split_state.rng[ray_index]);
		ASSIGN_RAY_STATE(kernel_split_state.ray_state, ray_index, RAY_TO_REGENERATE);
	}
}

CCL_NAMESPACE_END

