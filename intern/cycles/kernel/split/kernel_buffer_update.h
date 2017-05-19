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

/* This kernel takes care of rays that hit the background (sceneintersect
 * kernel), and for the rays of state RAY_UPDATE_BUFFER it updates the ray's
 * accumulated radiance in the output buffer. This kernel also takes care of
 * rays that have been determined to-be-regenerated.
 *
 * We will empty QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue in this kernel.
 *
 * Typically all rays that are in state RAY_HIT_BACKGROUND, RAY_UPDATE_BUFFER
 * will be eventually set to RAY_TO_REGENERATE state in this kernel.
 * Finally all rays of ray_state RAY_TO_REGENERATE will be regenerated and put
 * in queue QUEUE_ACTIVE_AND_REGENERATED_RAYS.
 *
 * State of queues when this kernel is called:
 * At entry,
 *   - QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE rays.
 *   - QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with
 *     RAY_UPDATE_BUFFER, RAY_HIT_BACKGROUND, RAY_TO_REGENERATE rays.
 * At exit,
 *   - QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE and
 *     RAY_REGENERATED rays.
 *   - QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be empty.
 */
ccl_device void kernel_buffer_update(KernelGlobals *kg,
                                     ccl_local_param unsigned int *local_queue_atomics)
{
	if(ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
		*local_queue_atomics = 0;
	}
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

	int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	if(ray_index == 0) {
		/* We will empty this queue in this kernel. */
		kernel_split_params.queue_index[QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS] = 0;
	}
	char enqueue_flag = 0;
	ray_index = get_ray_index(kg, ray_index,
	                          QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	                          kernel_split_state.queue_data,
	                          kernel_split_params.queue_size,
	                          1);

#ifdef __COMPUTE_DEVICE_GPU__
	/* If we are executing on a GPU device, we exit all threads that are not
	 * required.
	 *
	 * If we are executing on a CPU device, then we need to keep all threads
	 * active since we have barrier() calls later in the kernel. CPU devices,
	 * expect all threads to execute barrier statement.
	 */
	if(ray_index == QUEUE_EMPTY_SLOT) {
		return;
	}
#endif

#ifndef __COMPUTE_DEVICE_GPU__
	if(ray_index != QUEUE_EMPTY_SLOT) {
#endif

	ccl_global uint *rng_state = kernel_split_params.rng_state;
	int stride = kernel_split_params.stride;

	ccl_global char *ray_state = kernel_split_state.ray_state;
#ifdef __KERNEL_DEBUG__
	DebugData *debug_data = &kernel_split_state.debug_data[ray_index];
#endif
	ccl_global PathState *state = &kernel_split_state.path_state[ray_index];
	PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
	ccl_global Ray *ray = &kernel_split_state.ray[ray_index];
	ccl_global float3 *throughput = &kernel_split_state.throughput[ray_index];
	ccl_global float *L_transparent = &kernel_split_state.L_transparent[ray_index];
	RNG rng = kernel_split_state.rng[ray_index];
	ccl_global float *buffer = kernel_split_params.buffer;

	unsigned int work_index;
	ccl_global uint *initial_rng;

	unsigned int sample;
	unsigned int tile_x;
	unsigned int tile_y;
	unsigned int pixel_x;
	unsigned int pixel_y;

	work_index = kernel_split_state.work_array[ray_index];
	sample = get_work_sample(kg, work_index, ray_index) + kernel_split_params.start_sample;
	get_work_pixel_tile_position(kg, &pixel_x, &pixel_y,
	                        &tile_x, &tile_y,
	                        work_index,
	                        ray_index);
	initial_rng = rng_state;

	rng_state += kernel_split_params.offset + pixel_x + pixel_y*stride;
	buffer += (kernel_split_params.offset + pixel_x + pixel_y*stride) * kernel_data.film.pass_stride;

	if(IS_STATE(ray_state, ray_index, RAY_UPDATE_BUFFER)) {
#ifdef __KERNEL_DEBUG__
		kernel_write_debug_passes(kg, buffer, state, debug_data, sample);
#endif

		/* accumulate result in output buffer */
		bool is_shadow_catcher = (state->flag & PATH_RAY_SHADOW_CATCHER);
		kernel_write_result(kg, buffer, sample, L, 1.0f - (*L_transparent), is_shadow_catcher);

		path_rng_end(kg, rng_state, rng);

		ASSIGN_RAY_STATE(ray_state, ray_index, RAY_TO_REGENERATE);
	}

	if(IS_STATE(ray_state, ray_index, RAY_TO_REGENERATE)) {
		/* We have completed current work; So get next work */
		int valid_work = get_next_work(kg, &work_index, ray_index);
		if(!valid_work) {
			/* If work is invalid, this means no more work is available and the thread may exit */
			ASSIGN_RAY_STATE(ray_state, ray_index, RAY_INACTIVE);
		}

		if(IS_STATE(ray_state, ray_index, RAY_TO_REGENERATE)) {
			kernel_split_state.work_array[ray_index] = work_index;
			/* Get the sample associated with the current work */
			sample = get_work_sample(kg, work_index, ray_index) + kernel_split_params.start_sample;
			/* Get pixel and tile position associated with current work */
			get_work_pixel_tile_position(kg, &pixel_x, &pixel_y, &tile_x, &tile_y, work_index, ray_index);

			/* Remap rng_state according to the current work */
			rng_state = initial_rng + kernel_split_params.offset + pixel_x + pixel_y*stride;
			/* Remap buffer according to the current work */
			buffer += (kernel_split_params.offset + pixel_x + pixel_y*stride) * kernel_data.film.pass_stride;

			/* Initialize random numbers and ray. */
			kernel_path_trace_setup(kg, rng_state, sample, pixel_x, pixel_y, &rng, ray);

			if(ray->t != 0.0f) {
				/* Initialize throughput, L_transparent, Ray, PathState;
				 * These rays proceed with path-iteration.
				 */
				*throughput = make_float3(1.0f, 1.0f, 1.0f);
				*L_transparent = 0.0f;
				path_radiance_init(L, kernel_data.film.use_light_pass);
				path_state_init(kg, &kernel_split_state.sd_DL_shadow[ray_index], state, &rng, sample, ray);
#ifdef __SUBSURFACE__
				kernel_path_subsurface_init_indirect(&kernel_split_state.ss_rays[ray_index]);
#endif
#ifdef __KERNEL_DEBUG__
				debug_data_init(debug_data);
#endif
				ASSIGN_RAY_STATE(ray_state, ray_index, RAY_REGENERATED);
				enqueue_flag = 1;
			}
			else {
				/* These rays do not participate in path-iteration. */
				float4 L_rad = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
				/* Accumulate result in output buffer. */
				kernel_write_pass_float4(buffer, sample, L_rad);
				path_rng_end(kg, rng_state, rng);

				ASSIGN_RAY_STATE(ray_state, ray_index, RAY_TO_REGENERATE);
			}
		}
	}
	kernel_split_state.rng[ray_index] = rng;

#ifndef __COMPUTE_DEVICE_GPU__
	}
#endif

	/* Enqueue RAY_REGENERATED rays into QUEUE_ACTIVE_AND_REGENERATED_RAYS;
	 * These rays will be made active during next SceneIntersectkernel.
	 */
	enqueue_ray_index_local(ray_index,
	                        QUEUE_ACTIVE_AND_REGENERATED_RAYS,
	                        enqueue_flag,
	                        kernel_split_params.queue_size,
	                        local_queue_atomics,
	                        kernel_split_state.queue_data,
	                        kernel_split_params.queue_index);
}

CCL_NAMESPACE_END
