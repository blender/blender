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

#include "split/kernel_background_buffer_update.h"

__kernel void kernel_ocl_path_trace_background_buffer_update(
        ccl_global char *kg,
        ccl_constant KernelData *data,
        ccl_global float *per_sample_output_buffers,
        ccl_global uint *rng_state,
        ccl_global uint *rng_coop,             /* Required for buffer Update */
        ccl_global float3 *throughput_coop,    /* Required for background hit processing */
        PathRadiance *PathRadiance_coop,       /* Required for background hit processing and buffer Update */
        ccl_global Ray *Ray_coop,              /* Required for background hit processing */
        ccl_global PathState *PathState_coop,  /* Required for background hit processing */
        ccl_global float *L_transparent_coop,  /* Required for background hit processing and buffer Update */
        ccl_global char *ray_state,            /* Stores information on the current state of a ray */
        int sw, int sh, int sx, int sy, int stride,
        int rng_state_offset_x,
        int rng_state_offset_y,
        int rng_state_stride,
        ccl_global unsigned int *work_array,   /* Denotes work of each ray */
        ccl_global int *Queue_data,            /* Queues memory */
        ccl_global int *Queue_index,           /* Tracks the number of elements in each queue */
        int queuesize,                         /* Size (capacity) of each queue */
        int end_sample,
        int start_sample,
#ifdef __WORK_STEALING__
        ccl_global unsigned int *work_pool_wgs,
        unsigned int num_samples,
#endif
#ifdef __KERNEL_DEBUG__
        DebugData *debugdata_coop,
#endif
        int parallel_samples)                  /* Number of samples to be processed in parallel */
{
	ccl_local unsigned int local_queue_atomics;
	if(get_local_id(0) == 0 && get_local_id(1) == 0) {
		local_queue_atomics = 0;
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	int ray_index = get_global_id(1) * get_global_size(0) + get_global_id(0);
	if(ray_index == 0) {
		/* We will empty this queue in this kernel. */
		Queue_index[QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS] = 0;
	}
	char enqueue_flag = 0;
	ray_index = get_ray_index(ray_index,
	                          QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	                          Queue_data,
	                          queuesize,
	                          1);

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
		enqueue_flag =
			kernel_background_buffer_update((KernelGlobals *)kg,
			                                per_sample_output_buffers,
			                                rng_state,
			                                rng_coop,
			                                throughput_coop,
			                                PathRadiance_coop,
			                                Ray_coop,
			                                PathState_coop,
			                                L_transparent_coop,
			                                ray_state,
			                                sw, sh, sx, sy, stride,
			                                rng_state_offset_x,
			                                rng_state_offset_y,
			                                rng_state_stride,
			                                work_array,
			                                end_sample,
			                                start_sample,
#ifdef __WORK_STEALING__
			                                work_pool_wgs,
			                                num_samples,
#endif
#ifdef __KERNEL_DEBUG__
			                                debugdata_coop,
#endif
			                                parallel_samples,
			                                ray_index);
#ifndef __COMPUTE_DEVICE_GPU__
	}
#endif

	/* Enqueue RAY_REGENERATED rays into QUEUE_ACTIVE_AND_REGENERATED_RAYS;
	 * These rays will be made active during next SceneIntersectkernel.
	 */
	enqueue_ray_index_local(ray_index,
	                        QUEUE_ACTIVE_AND_REGENERATED_RAYS,
	                        enqueue_flag,
	                        queuesize,
	                        &local_queue_atomics,
	                        Queue_data,
	                        Queue_index);
}
