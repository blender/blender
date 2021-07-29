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

/* This kernel operates on QUEUE_ACTIVE_AND_REGENERATED_RAYS.
 * It processes rays of state RAY_ACTIVE and RAY_HIT_BACKGROUND.
 * We will empty QUEUE_ACTIVE_AND_REGENERATED_RAYS queue in this kernel.
 */
ccl_device void kernel_lamp_emission(KernelGlobals *kg)
{
#ifndef __VOLUME__
	/* We will empty this queue in this kernel. */
	if(ccl_global_id(0) == 0 && ccl_global_id(1) == 0) {
		kernel_split_params.queue_index[QUEUE_ACTIVE_AND_REGENERATED_RAYS] = 0;
	}
#endif
	/* Fetch use_queues_flag. */
	char local_use_queues_flag = *kernel_split_params.use_queues_flag;
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

	int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	if(local_use_queues_flag) {
		ray_index = get_ray_index(kg, ray_index,
		                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
		                          kernel_split_state.queue_data,
		                          kernel_split_params.queue_size,
#ifndef __VOLUME__
		                          1
#else
		                          0
#endif
		                          );
		if(ray_index == QUEUE_EMPTY_SLOT) {
			return;
		}
	}

	if(IS_STATE(kernel_split_state.ray_state, ray_index, RAY_ACTIVE) ||
	   IS_STATE(kernel_split_state.ray_state, ray_index, RAY_HIT_BACKGROUND))
	{
		PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
		ccl_global PathState *state = &kernel_split_state.path_state[ray_index];

		float3 throughput = kernel_split_state.throughput[ray_index];
		Ray ray = kernel_split_state.ray[ray_index];

#ifdef __LAMP_MIS__
		if(kernel_data.integrator.use_lamp_mis && !(state->flag & PATH_RAY_CAMERA)) {
			/* ray starting from previous non-transparent bounce */
			Ray light_ray;

			light_ray.P = ray.P - state->ray_t*ray.D;
			state->ray_t += kernel_split_state.isect[ray_index].t;
			light_ray.D = ray.D;
			light_ray.t = state->ray_t;
			light_ray.time = ray.time;
			light_ray.dD = ray.dD;
			light_ray.dP = ray.dP;
			/* intersect with lamp */
			float3 emission;

			if(indirect_lamp_emission(kg, &kernel_split_state.sd_DL_shadow[ray_index], state, &light_ray, &emission)) {
				path_radiance_accum_emission(L, throughput, emission, state->bounce);
			}
		}
#endif  /* __LAMP_MIS__ */
	}
}

CCL_NAMESPACE_END

