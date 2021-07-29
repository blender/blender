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

/* This kernel sets up the ShaderData structure from the values computed
 * by the previous kernels.
 *
 * It also identifies the rays of state RAY_TO_REGENERATE and enqueues them
 * in QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue.
 */
ccl_device void kernel_shader_setup(KernelGlobals *kg,
                                    ccl_local_param unsigned int *local_queue_atomics)
{
	/* Enqeueue RAY_TO_REGENERATE rays into QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue. */
	if(ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
		*local_queue_atomics = 0;
	}
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

	int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	int queue_index = kernel_split_params.queue_index[QUEUE_ACTIVE_AND_REGENERATED_RAYS];
	if(ray_index >= queue_index) {
		return;
	}
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
	                        local_queue_atomics,
	                        kernel_split_state.queue_data,
	                        kernel_split_params.queue_index);

	/* Continue on with shader evaluation. */
	if(IS_STATE(kernel_split_state.ray_state, ray_index, RAY_ACTIVE)) {
		Intersection isect = kernel_split_state.isect[ray_index];
		Ray ray = kernel_split_state.ray[ray_index];

		shader_setup_from_ray(kg,
		                      &kernel_split_state.sd[ray_index],
		                      &isect,
		                      &ray);
	}
}

CCL_NAMESPACE_END
