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

#include "split/kernel_lamp_emission.h"

__kernel void kernel_ocl_path_trace_lamp_emission(
        ccl_global char *kg,
        ccl_constant KernelData *data,
        ccl_global float3 *throughput_coop,    /* Required for lamp emission */
        PathRadiance *PathRadiance_coop,       /* Required for lamp emission */
        ccl_global Ray *Ray_coop,              /* Required for lamp emission */
        ccl_global PathState *PathState_coop,  /* Required for lamp emission */
        Intersection *Intersection_coop,       /* Required for lamp emission */
        ccl_global char *ray_state,            /* Denotes the state of each ray */
        int sw, int sh,
        ccl_global int *Queue_data,            /* Memory for queues */
        ccl_global int *Queue_index,           /* Tracks the number of elements in queues */
        int queuesize,                         /* Size (capacity) of queues */
        ccl_global char *use_queues_flag,      /* Used to decide if this kernel should use
                                                * queues to fetch ray index
                                                */
        int parallel_samples)                  /* Number of samples to be processed in parallel */
{
	int x = get_global_id(0);
	int y = get_global_id(1);

	/* We will empty this queue in this kernel. */
	if(get_global_id(0) == 0 && get_global_id(1) == 0) {
		Queue_index[QUEUE_ACTIVE_AND_REGENERATED_RAYS] = 0;
	}
	/* Fetch use_queues_flag. */
	ccl_local char local_use_queues_flag;
	if(get_local_id(0) == 0 && get_local_id(1) == 0) {
		local_use_queues_flag = use_queues_flag[0];
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	int ray_index;
	if(local_use_queues_flag) {
		int thread_index = get_global_id(1) * get_global_size(0) + get_global_id(0);
		ray_index = get_ray_index(thread_index,
		                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
		                          Queue_data,
		                          queuesize,
		                          1);
		if(ray_index == QUEUE_EMPTY_SLOT) {
			return;
		}
	} else {
		if(x < (sw * parallel_samples) && y < sh) {
			ray_index = x + y * (sw * parallel_samples);
		} else {
			return;
		}
	}

	kernel_lamp_emission((KernelGlobals *)kg,
	                     throughput_coop,
	                     PathRadiance_coop,
	                     Ray_coop,
	                     PathState_coop,
	                     Intersection_coop,
	                     ray_state,
	                     sw, sh,
	                     use_queues_flag,
	                     ray_index);
}
