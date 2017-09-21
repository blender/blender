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

	/* Get work. */
	uint work_index;
	if(!get_next_work(kg, ray_index, &work_index)) {
		/* No more work, mark ray as inactive */
		kernel_split_state.ray_state[ray_index] = RAY_INACTIVE;

		return;
	}

	uint x, y, sample;
	get_work_pixel(kg, work_index, &x, &y, &sample);

	/* Remap rng_state and buffer to current pixel. */
	ccl_global uint *rng_state = kernel_split_params.rng_state;
	rng_state += kernel_split_params.offset + x + y*kernel_split_params.stride;

	/* Store buffer offset for writing to passes. */
	uint buffer_offset = (kernel_split_params.offset + x + y*kernel_split_params.stride) * kernel_data.film.pass_stride;
	kernel_split_state.buffer_offset[ray_index] = buffer_offset;

	/* Initialize random numbers and ray. */
	uint rng_hash;
	kernel_path_trace_setup(kg,
	                        rng_state,
	                        sample,
	                        x, y,
	                        &rng_hash,
	                        &kernel_split_state.ray[ray_index]);

	if(kernel_split_state.ray[ray_index].t != 0.0f) {
		/* Initialize throughput, path radiance, Ray, PathState;
		 * These rays proceed with path-iteration.
		 */
		kernel_split_state.throughput[ray_index] = make_float3(1.0f, 1.0f, 1.0f);
		path_radiance_init(&kernel_split_state.path_radiance[ray_index], kernel_data.film.use_light_pass);
		path_state_init(kg,
		                &kernel_split_state.sd_DL_shadow[ray_index],
		                &kernel_split_state.path_state[ray_index],
		                rng_hash,
		                sample,
		                &kernel_split_state.ray[ray_index]);
#ifdef __SUBSURFACE__
		kernel_path_subsurface_init_indirect(&kernel_split_state.ss_rays[ray_index]);
#endif
	}
	else {
		/* These rays do not participate in path-iteration. */
		float4 L_rad = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
		/* Accumulate result in output buffer. */
		ccl_global float *buffer = kernel_split_params.buffer + buffer_offset;
		kernel_write_pass_float4(buffer, sample, L_rad);
		ASSIGN_RAY_STATE(kernel_split_state.ray_state, ray_index, RAY_TO_REGENERATE);
	}
}

CCL_NAMESPACE_END
