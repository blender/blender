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

/* Shadow ray cast for AO. */
ccl_device void kernel_shadow_blocked_ao(KernelGlobals *kg)
{
	unsigned int ao_queue_length = kernel_split_params.queue_index[QUEUE_SHADOW_RAY_CAST_AO_RAYS];
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

	int ray_index = QUEUE_EMPTY_SLOT;
	int thread_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	if(thread_index < ao_queue_length) {
		ray_index = get_ray_index(kg, thread_index, QUEUE_SHADOW_RAY_CAST_AO_RAYS,
		                          kernel_split_state.queue_data, kernel_split_params.queue_size, 1);
	}

	if(ray_index == QUEUE_EMPTY_SLOT)
		return;

	/* Flag determining if we need to update L. */
	char update_path_radiance = 0;

	if(IS_FLAG(kernel_split_state.ray_state, ray_index, RAY_SHADOW_RAY_CAST_AO)) {
		ccl_global PathState *state = &kernel_split_state.path_state[ray_index];
		ccl_global Ray *light_ray_global = &kernel_split_state.ao_light_ray[ray_index];

		float3 shadow;
		Ray ray = *light_ray_global;
		update_path_radiance = !(shadow_blocked(kg,
		                                        &kernel_split_state.sd_DL_shadow[ray_index],
		                                        state,
		                                        &ray,
		                                        &shadow));

		*light_ray_global = ray;
		/* We use light_ray_global's P and t to store shadow and
		 * update_path_radiance.
		 */
		light_ray_global->P = shadow;
		light_ray_global->t = update_path_radiance;
	}
}

CCL_NAMESPACE_END

