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

#include "split/kernel_shader_eval.h"

__kernel void kernel_ocl_path_trace_shader_eval(
        ccl_global char *kg,
        ccl_constant KernelData *data,
        ccl_global char *sd,                   /* Output ShaderData structure to be filled */
        ccl_global uint *rng_coop,             /* Required for rbsdf calculation */
        ccl_global Ray *Ray_coop,              /* Required for setting up shader from ray */
        ccl_global PathState *PathState_coop,  /* Required for all functions in this kernel */
        Intersection *Intersection_coop,       /* Required for setting up shader from ray */
        ccl_global char *ray_state,            /* Denotes the state of each ray */
        ccl_global int *Queue_data,            /* queue memory */
        ccl_global int *Queue_index,           /* Tracks the number of elements in each queue */
        int queuesize)                         /* Size (capacity) of each queue */
{
	/* Enqeueue RAY_TO_REGENERATE rays into QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue. */
	ccl_local unsigned int local_queue_atomics;
	if(get_local_id(0) == 0 && get_local_id(1) == 0) {
		local_queue_atomics = 0;
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	int ray_index = get_global_id(1) * get_global_size(0) + get_global_id(0);
	ray_index = get_ray_index(ray_index,
	                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
	                          Queue_data,
	                          queuesize,
	                          0);

	if(ray_index == QUEUE_EMPTY_SLOT) {
		return;
	}

	char enqueue_flag = (IS_STATE(ray_state, ray_index, RAY_TO_REGENERATE)) ? 1 : 0;
	enqueue_ray_index_local(ray_index,
	                        QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	                        enqueue_flag,
	                        queuesize,
	                        &local_queue_atomics,
	                        Queue_data,
	                        Queue_index);

	/* Continue on with shader evaluation. */
	kernel_shader_eval((KernelGlobals *)kg,
	                   (ShaderData *)sd,
	                   rng_coop,
	                   Ray_coop,
	                   PathState_coop,
	                   Intersection_coop,
	                   ray_state,
	                   ray_index);
}
