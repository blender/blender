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
	ccl_global char *ray_state = kernel_split_state.ray_state;

	int thread_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	int ray_index;

	if(kernel_data.integrator.ao_bounces != INT_MAX) {
		ray_index = get_ray_index(kg, thread_index,
		                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
		                          kernel_split_state.queue_data,
		                          kernel_split_params.queue_size,
		                          0);

		if(ray_index != QUEUE_EMPTY_SLOT) {
			if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
				ccl_global PathState *state = &kernel_split_state.path_state[ray_index];
				if(state->bounce > kernel_data.integrator.ao_bounces) {
					kernel_split_path_end(kg, ray_index);
				}
			}
		}
	}

	ray_index = get_ray_index(kg, thread_index,
	                          QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	                          kernel_split_state.queue_data,
	                          kernel_split_params.queue_size,
	                          0);

	if(ray_index == QUEUE_EMPTY_SLOT) {
		return;
	}

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
				kernel_split_path_end(kg, ray_index);
		}

		if(IS_STATE(ray_state, ray_index, RAY_HIT_BACKGROUND)) {
#ifdef __BACKGROUND__
			/* sample background shader */
			float3 L_background = indirect_background(kg, &kernel_split_state.sd_DL_shadow[ray_index], state, ray);
			path_radiance_accum_background(L, state, (*throughput), L_background);
#endif
			kernel_split_path_end(kg, ray_index);
		}
	}


}

CCL_NAMESPACE_END
