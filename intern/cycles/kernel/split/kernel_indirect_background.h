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

ccl_device void kernel_indirect_background(KernelGlobals *kg)
{
	/*
	ccl_local unsigned int local_queue_atomics;
	if(ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
		local_queue_atomics = 0;
	}
	ccl_barrier(CCL_LOCAL_MEM_FENCE);
	// */

	int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	ray_index = get_ray_index(kg, ray_index,
	                          QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	                          kernel_split_state.queue_data,
	                          kernel_split_params.queue_size,
	                          0);

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


	ccl_global char *ray_state = kernel_split_state.ray_state;
	ccl_global PathState *state = &kernel_split_state.path_state[ray_index];
	PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
	ccl_global Ray *ray = &kernel_split_state.ray[ray_index];
	ccl_global float3 *throughput = &kernel_split_state.throughput[ray_index];
	ccl_global float *L_transparent = &kernel_split_state.L_transparent[ray_index];

	if(IS_STATE(ray_state, ray_index, RAY_HIT_BACKGROUND)) {
		/* eval background shader if nothing hit */
		if(kernel_data.background.transparent && (state->flag & PATH_RAY_CAMERA)) {
			*L_transparent = (*L_transparent) + average((*throughput));
#ifdef __PASSES__
			if(!(kernel_data.film.pass_flag & PASS_BACKGROUND))
#endif
				ASSIGN_RAY_STATE(ray_state, ray_index, RAY_UPDATE_BUFFER);
		}

		if(IS_STATE(ray_state, ray_index, RAY_HIT_BACKGROUND)) {
#ifdef __BACKGROUND__
			/* sample background shader */
			float3 L_background = indirect_background(kg, &kernel_split_state.sd_DL_shadow[ray_index], state, ray);
			path_radiance_accum_background(L, (*throughput), L_background, state->bounce);
#endif
			ASSIGN_RAY_STATE(ray_state, ray_index, RAY_UPDATE_BUFFER);
		}
	}

#ifndef __COMPUTE_DEVICE_GPU__
	}
#endif

}

CCL_NAMESPACE_END
