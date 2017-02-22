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

/* Note on kernel_shader_eval kernel
 * This kernel is the 5th kernel in the ray tracing logic. This is
 * the 4rd kernel in path iteration. This kernel sets up the ShaderData
 * structure from the values computed by the previous kernels. It also identifies
 * the rays of state RAY_TO_REGENERATE and enqueues them in QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue.
 *
 * The input and output of the kernel is as follows,
 * rng_coop -------------------------------------------|--- kernel_shader_eval --|--- sd
 * Ray_coop -------------------------------------------|                         |--- Queue_data (QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS)
 * PathState_coop -------------------------------------|                         |--- Queue_index (QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS)
 * Intersection_coop ----------------------------------|                         |
 * Queue_data (QUEUE_ACTIVE_AND_REGENERATD_RAYS)-------|                         |
 * Queue_index(QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS)---|                         |
 * ray_state ------------------------------------------|                         |
 * kg (globals) ---------------------------------------|                         |
 * queuesize ------------------------------------------|                         |
 *
 * Note on Queues :
 * This kernel reads from the QUEUE_ACTIVE_AND_REGENERATED_RAYS queue and processes
 * only the rays of state RAY_ACTIVE;
 * State of queues when this kernel is called,
 * at entry,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE and RAY_REGENERATED rays
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be empty.
 * at exit,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE and RAY_REGENERATED rays
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with RAY_TO_REGENERATE rays
 */

ccl_device void kernel_shader_eval(KernelGlobals *kg)
{
	/* Enqeueue RAY_TO_REGENERATE rays into QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue. */
	ccl_local unsigned int local_queue_atomics;
	if(ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
		local_queue_atomics = 0;
	}
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

	int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	ray_index = get_ray_index(kg, ray_index,
	                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
	                          kernel_split_state.queue_data,
	                          kernel_split_params.queue_size,
	                          0);

	if(ray_index == QUEUE_EMPTY_SLOT) {
		return;
	}

	char enqueue_flag = (IS_STATE(kernel_split_state.ray_state, ray_index, RAY_TO_REGENERATE)) ? 1 : 0;
	enqueue_ray_index_local(ray_index,
	                        QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	                        enqueue_flag,
	                        kernel_split_params.queue_size,
	                        &local_queue_atomics,
	                        kernel_split_state.queue_data,
	                        kernel_split_params.queue_index);

	/* Continue on with shader evaluation. */
	if(IS_STATE(kernel_split_state.ray_state, ray_index, RAY_ACTIVE)) {
		Intersection *isect = &kernel_split_state.isect[ray_index];
		ccl_global uint *rng = &kernel_split_state.rng[ray_index];
		ccl_global PathState *state = &kernel_split_state.path_state[ray_index];
		Ray ray = kernel_split_state.ray[ray_index];

		shader_setup_from_ray(kg,
		                      kernel_split_state.sd,
		                      isect,
		                      &ray);
		float rbsdf = path_state_rng_1D_for_decision(kg, rng, state, PRNG_BSDF);
		shader_eval_surface(kg, kernel_split_state.sd, rng, state, rbsdf, state->flag, SHADER_CONTEXT_MAIN);
	}
}

CCL_NAMESPACE_END

