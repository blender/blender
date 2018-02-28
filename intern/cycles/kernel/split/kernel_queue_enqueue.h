/*
 * Copyright 2011-2016 Blender Foundation
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

/* This kernel enqueues rays of different ray state into their
 * appropriate queues:
 *
 * 1. Rays that have been determined to hit the background from the
 *    "kernel_scene_intersect" kernel are enqueued in
 *    QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS;
 * 2. Rays that have been determined to be actively participating in pat
 *    -iteration will be enqueued into QUEUE_ACTIVE_AND_REGENERATED_RAYS.
 *
 * State of queue during other times this kernel is called:
 * At entry,
 *   - QUEUE_ACTIVE_AND_REGENERATED_RAYS will be empty.
 *   - QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will contain RAY_TO_REGENERATE
 *     and RAY_UPDATE_BUFFER rays.
 * At exit,
 *   - QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE rays.
 *   - QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with
 *     RAY_TO_REGENERATE, RAY_UPDATE_BUFFER, RAY_HIT_BACKGROUND rays.
 */
ccl_device void kernel_queue_enqueue(KernelGlobals *kg,
                                     ccl_local_param QueueEnqueueLocals *locals)
{
	/* We have only 2 cases (Hit/Not-Hit) */
	int lidx = ccl_local_id(1) * ccl_local_size(0) + ccl_local_id(0);
	int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);

	if(lidx == 0) {
		locals->queue_atomics[0] = 0;
		locals->queue_atomics[1] = 0;
	}
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

	int queue_number = -1;

	if(IS_STATE(kernel_split_state.ray_state, ray_index, RAY_HIT_BACKGROUND) ||
	   IS_STATE(kernel_split_state.ray_state, ray_index, RAY_UPDATE_BUFFER) ||
	   IS_STATE(kernel_split_state.ray_state, ray_index, RAY_TO_REGENERATE)) {
		queue_number = QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS;
	}
	else if(IS_STATE(kernel_split_state.ray_state, ray_index, RAY_ACTIVE) ||
	        IS_STATE(kernel_split_state.ray_state, ray_index, RAY_HAS_ONLY_VOLUME) ||
	        IS_STATE(kernel_split_state.ray_state, ray_index, RAY_REGENERATED)) {
		queue_number = QUEUE_ACTIVE_AND_REGENERATED_RAYS;
	}

	unsigned int my_lqidx;
	if(queue_number != -1) {
		my_lqidx = get_local_queue_index(queue_number, locals->queue_atomics);
	}
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

	if(lidx == 0) {
		locals->queue_atomics[QUEUE_ACTIVE_AND_REGENERATED_RAYS] =
		        get_global_per_queue_offset(QUEUE_ACTIVE_AND_REGENERATED_RAYS,
		                                    locals->queue_atomics,
		                                    kernel_split_params.queue_index);
		locals->queue_atomics[QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS] =
		        get_global_per_queue_offset(QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
		                                    locals->queue_atomics,
		                                    kernel_split_params.queue_index);
	}
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

	unsigned int my_gqidx;
	if(queue_number != -1) {
		my_gqidx = get_global_queue_index(queue_number,
		                                  kernel_split_params.queue_size,
		                                  my_lqidx,
		                                  locals->queue_atomics);
		kernel_split_state.queue_data[my_gqidx] = ray_index;
	}
}

CCL_NAMESPACE_END
