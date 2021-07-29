/*
 * Copyright 2011-2017 Blender Foundation
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

/* This kernel initializes structures needed in path-iteration kernels.
 * This is the first kernel in ray-tracing logic.
 *
 * Ray state of rays outside the tile-boundary will be marked RAY_INACTIVE
 */
ccl_device void kernel_path_init(KernelGlobals *kg) {
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

	unsigned int work_index = 0;
	/* Get work. */
	if(!get_next_work(kg, &work_index, ray_index)) {
		/* No more work, mark ray as inactive */
		kernel_split_state.ray_state[ray_index] = RAY_INACTIVE;

		return;
	}

	/* Get the sample associated with the work. */
	my_sample = get_work_sample(kg, work_index, ray_index) + kernel_split_params.start_sample;

	/* Get pixel and tile position associated with the work. */
	get_work_pixel_tile_position(kg, &pixel_x, &pixel_y,
	                             &tile_x, &tile_y,
	                             work_index,
	                             ray_index);
	kernel_split_state.work_array[ray_index] = work_index;

	ccl_global uint *rng_state = kernel_split_params.rng_state;
	rng_state += kernel_split_params.offset + pixel_x + pixel_y*kernel_split_params.stride;

	ccl_global float *buffer = kernel_split_params.buffer;
	buffer += (kernel_split_params.offset + pixel_x + pixel_y * kernel_split_params.stride) * kernel_data.film.pass_stride;

	RNG rng = kernel_split_state.rng[ray_index];

	/* Initialize random numbers and ray. */
	kernel_path_trace_setup(kg,
	                        rng_state,
	                        my_sample,
	                        pixel_x, pixel_y,
	                        &rng,
	                        &kernel_split_state.ray[ray_index]);

	if(kernel_split_state.ray[ray_index].t != 0.0f) {
		/* Initialize throughput, L_transparent, Ray, PathState;
		 * These rays proceed with path-iteration.
		 */
		kernel_split_state.throughput[ray_index] = make_float3(1.0f, 1.0f, 1.0f);
		kernel_split_state.L_transparent[ray_index] = 0.0f;
		path_radiance_init(&kernel_split_state.path_radiance[ray_index], kernel_data.film.use_light_pass);
		path_state_init(kg,
		                &kernel_split_state.sd_DL_shadow[ray_index],
		                &kernel_split_state.path_state[ray_index],
		                &rng,
		                my_sample,
		                &kernel_split_state.ray[ray_index]);
#ifdef __SUBSURFACE__
		kernel_path_subsurface_init_indirect(&kernel_split_state.ss_rays[ray_index]);
#endif

#ifdef __KERNEL_DEBUG__
		debug_data_init(&kernel_split_state.debug_data[ray_index]);
#endif
	}
	else {
		/* These rays do not participate in path-iteration. */
		float4 L_rad = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
		/* Accumulate result in output buffer. */
		kernel_write_pass_float4(buffer, my_sample, L_rad);
		path_rng_end(kg, rng_state, kernel_split_state.rng[ray_index]);
		ASSIGN_RAY_STATE(kernel_split_state.ray_state, ray_index, RAY_TO_REGENERATE);
	}
	kernel_split_state.rng[ray_index] = rng;
}

CCL_NAMESPACE_END
