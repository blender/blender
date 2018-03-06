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

ccl_device void kernel_indirect_subsurface(KernelGlobals *kg)
{
	int thread_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	if(thread_index == 0) {
		/* We will empty both queues in this kernel. */
		kernel_split_params.queue_index[QUEUE_ACTIVE_AND_REGENERATED_RAYS] = 0;
		kernel_split_params.queue_index[QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS] = 0;
	}

	int ray_index;
	get_ray_index(kg, thread_index,
	              QUEUE_ACTIVE_AND_REGENERATED_RAYS,
	              kernel_split_state.queue_data,
	              kernel_split_params.queue_size,
	              1);
	ray_index = get_ray_index(kg, thread_index,
	                          QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	                          kernel_split_state.queue_data,
	                          kernel_split_params.queue_size,
	                          1);

#ifdef __SUBSURFACE__
	if(ray_index == QUEUE_EMPTY_SLOT) {
		return;
	}

	ccl_global char *ray_state = kernel_split_state.ray_state;
	ccl_global PathState *state = &kernel_split_state.path_state[ray_index];
	PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
	ccl_global Ray *ray = &kernel_split_state.ray[ray_index];
	ccl_global float3 *throughput = &kernel_split_state.throughput[ray_index];

	if(IS_STATE(ray_state, ray_index, RAY_UPDATE_BUFFER)) {
		ccl_addr_space SubsurfaceIndirectRays *ss_indirect = &kernel_split_state.ss_rays[ray_index];

		/* Trace indirect subsurface rays by restarting the loop. this uses less
		 * stack memory than invoking kernel_path_indirect.
		 */
		if(ss_indirect->num_rays) {
			kernel_path_subsurface_setup_indirect(kg,
			                                      ss_indirect,
			                                      state,
			                                      ray,
			                                      L,
			                                      throughput);
			ASSIGN_RAY_STATE(ray_state, ray_index, RAY_REGENERATED);
		}
	}
#endif  /* __SUBSURFACE__ */
}

CCL_NAMESPACE_END
