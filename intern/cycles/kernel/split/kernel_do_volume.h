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


ccl_device void kernel_do_volume(KernelGlobals *kg)
{
#ifdef __VOLUME__
	/* We will empty this queue in this kernel. */
	if(ccl_global_id(0) == 0 && ccl_global_id(1) == 0) {
		kernel_split_params.queue_index[QUEUE_ACTIVE_AND_REGENERATED_RAYS] = 0;
	}
	/* Fetch use_queues_flag. */
	char local_use_queues_flag = *kernel_split_params.use_queues_flag;
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

	int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	if(local_use_queues_flag) {
		ray_index = get_ray_index(kg, ray_index,
		                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
		                          kernel_split_state.queue_data,
		                          kernel_split_params.queue_size,
		                          1);
		if(ray_index == QUEUE_EMPTY_SLOT) {
			return;
		}
	}

	if(IS_STATE(kernel_split_state.ray_state, ray_index, RAY_ACTIVE) ||
	   IS_STATE(kernel_split_state.ray_state, ray_index, RAY_HIT_BACKGROUND)) {

		bool hit = ! IS_STATE(kernel_split_state.ray_state, ray_index, RAY_HIT_BACKGROUND);

		PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
		ccl_global PathState *state = &kernel_split_state.path_state[ray_index];

		ccl_global float3 *throughput = &kernel_split_state.throughput[ray_index];
		ccl_global Ray *ray = &kernel_split_state.ray[ray_index];
		ccl_global RNG *rng = &kernel_split_state.rng[ray_index];
		ccl_global Intersection *isect = &kernel_split_state.isect[ray_index];
		ShaderData *sd = &kernel_split_state.sd[ray_index];
		ShaderData *sd_input = &kernel_split_state.sd_DL_shadow[ray_index];

		/* Sanitize volume stack. */
		if(!hit) {
			kernel_volume_clean_stack(kg, state->volume_stack);
		}
		/* volume attenuation, emission, scatter */
		if(state->volume_stack[0].shader != SHADER_NONE) {
			Ray volume_ray = *ray;
			volume_ray.t = (hit)? isect->t: FLT_MAX;

			bool heterogeneous = volume_stack_is_heterogeneous(kg, state->volume_stack);

			{
				/* integrate along volume segment with distance sampling */
				VolumeIntegrateResult result = kernel_volume_integrate(
					kg, state, sd, &volume_ray, L, throughput, rng, heterogeneous);

#  ifdef __VOLUME_SCATTER__
				if(result == VOLUME_PATH_SCATTERED) {
					/* direct lighting */
					kernel_path_volume_connect_light(kg, rng, sd, sd_input, *throughput, state, L);

					/* indirect light bounce */
					if(kernel_path_volume_bounce(kg, rng, sd, throughput, state, L, ray))
						ASSIGN_RAY_STATE(kernel_split_state.ray_state, ray_index, RAY_REGENERATED);
					else
						ASSIGN_RAY_STATE(kernel_split_state.ray_state, ray_index, RAY_UPDATE_BUFFER);
				}
#  endif
			}
		}
	}

#endif
}


CCL_NAMESPACE_END
