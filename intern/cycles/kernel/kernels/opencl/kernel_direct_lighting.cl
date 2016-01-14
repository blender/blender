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

#include "split/kernel_direct_lighting.h"

__kernel void kernel_ocl_path_trace_direct_lighting(
        ccl_global char *kg,
        ccl_constant KernelData *data,
        ccl_global char *sd,                    /* Required for direct lighting */
        ccl_global uint *rng_coop,              /* Required for direct lighting */
        ccl_global PathState *PathState_coop,   /* Required for direct lighting */
        ccl_global int *ISLamp_coop,            /* Required for direct lighting */
        ccl_global Ray *LightRay_coop,          /* Required for direct lighting */
        ccl_global BsdfEval *BSDFEval_coop,     /* Required for direct lighting */
        ccl_global char *ray_state,             /* Denotes the state of each ray */
        ccl_global int *Queue_data,             /* Queue memory */
        ccl_global int *Queue_index,            /* Tracks the number of elements in each queue */
        int queuesize)                          /* Size (capacity) of each queue */
{
	ccl_local unsigned int local_queue_atomics;
	if(get_local_id(0) == 0 && get_local_id(1) == 0) {
		local_queue_atomics = 0;
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	char enqueue_flag = 0;
	int ray_index = get_global_id(1) * get_global_size(0) + get_global_id(0);
	ray_index = get_ray_index(ray_index,
	                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
	                          Queue_data,
	                          queuesize,
	                          0);

#ifdef __COMPUTE_DEVICE_GPU__
	/* If we are executing on a GPU device, we exit all threads that are not
	 * required.
	 *
	 * If we are executing on a CPU device, then we need to keep all threads
	 * active since we have barrier() calls later in the kernel. CPU devices,
	 * expect all threads to execute barrier statement.
	 */
	if(ray_index == QUEUE_EMPTY_SLOT) {
		return;
	}
#endif

#ifndef __COMPUTE_DEVICE_GPU__
	if(ray_index != QUEUE_EMPTY_SLOT) {
#endif
		enqueue_flag = kernel_direct_lighting((KernelGlobals *)kg,
		                                      (ShaderData *)sd,
		                                      rng_coop,
		                                      PathState_coop,
		                                      ISLamp_coop,
		                                      LightRay_coop,
		                                      BSDFEval_coop,
		                                      ray_state,
		                                      ray_index);

#ifndef __COMPUTE_DEVICE_GPU__
	}
#endif

#ifdef __EMISSION__
	/* Enqueue RAY_SHADOW_RAY_CAST_DL rays. */
	enqueue_ray_index_local(ray_index,
	                        QUEUE_SHADOW_RAY_CAST_DL_RAYS,
	                        enqueue_flag,
	                        queuesize,
	                        &local_queue_atomics,
	                        Queue_data,
	                        Queue_index);
#endif
}
