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

#include "split/kernel_holdout_emission_blurring_pathtermination_ao.h"

__kernel void kernel_ocl_path_trace_holdout_emission_blurring_pathtermination_ao(
        ccl_global char *kg,
        ccl_constant KernelData *data,
        ccl_global char *sd,                   /* Required throughout the kernel except probabilistic path termination and AO */
        ccl_global float *per_sample_output_buffers,
        ccl_global uint *rng_coop,             /* Required for "kernel_write_data_passes" and AO */
        ccl_global float3 *throughput_coop,    /* Required for handling holdout material and AO */
        ccl_global float *L_transparent_coop,  /* Required for handling holdout material */
        PathRadiance *PathRadiance_coop,       /* Required for "kernel_write_data_passes" and indirect primitive emission */
        ccl_global PathState *PathState_coop,  /* Required throughout the kernel and AO */
        Intersection *Intersection_coop,       /* Required for indirect primitive emission */
        ccl_global float3 *AOAlpha_coop,       /* Required for AO */
        ccl_global float3 *AOBSDF_coop,        /* Required for AO */
        ccl_global Ray *AOLightRay_coop,       /* Required for AO */
        int sw, int sh, int sx, int sy, int stride,
        ccl_global char *ray_state,            /* Denotes the state of each ray */
        ccl_global unsigned int *work_array,   /* Denotes the work that each ray belongs to */
        ccl_global int *Queue_data,            /* Queue memory */
        ccl_global int *Queue_index,           /* Tracks the number of elements in each queue */
        int queuesize,                         /* Size (capacity) of each queue */
#ifdef __WORK_STEALING__
        unsigned int start_sample,
#endif
        int parallel_samples)                  /* Number of samples to be processed in parallel */
{
	ccl_local unsigned int local_queue_atomics_bg;
	ccl_local unsigned int local_queue_atomics_ao;
	if(get_local_id(0) == 0 && get_local_id(1) == 0) {
		local_queue_atomics_bg = 0;
		local_queue_atomics_ao = 0;
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	char enqueue_flag = 0;
	char enqueue_flag_AO_SHADOW_RAY_CAST = 0;
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
#endif  /* __COMPUTE_DEVICE_GPU__ */

#ifndef __COMPUTE_DEVICE_GPU__
	if(ray_index != QUEUE_EMPTY_SLOT) {
#endif
		kernel_holdout_emission_blurring_pathtermination_ao(
		        (KernelGlobals *)kg,
		        (ShaderData *)sd,
		        per_sample_output_buffers,
		        rng_coop,
		        throughput_coop,
		        L_transparent_coop,
		        PathRadiance_coop,
		        PathState_coop,
		        Intersection_coop,
		        AOAlpha_coop,
		        AOBSDF_coop,
		        AOLightRay_coop,
		        sw, sh, sx, sy, stride,
		        ray_state,
		        work_array,
#ifdef __WORK_STEALING__
		        start_sample,
#endif
		        parallel_samples,
		        ray_index,
		        &enqueue_flag,
		        &enqueue_flag_AO_SHADOW_RAY_CAST);
#ifndef __COMPUTE_DEVICE_GPU__
	}
#endif

	/* Enqueue RAY_UPDATE_BUFFER rays. */
	enqueue_ray_index_local(ray_index,
	                        QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	                        enqueue_flag,
	                        queuesize,
	                        &local_queue_atomics_bg,
	                        Queue_data,
	                        Queue_index);

#ifdef __AO__
	/* Enqueue to-shadow-ray-cast rays. */
	enqueue_ray_index_local(ray_index,
	                        QUEUE_SHADOW_RAY_CAST_AO_RAYS,
	                        enqueue_flag_AO_SHADOW_RAY_CAST,
	                        queuesize,
	                        &local_queue_atomics_ao,
	                        Queue_data,
	                        Queue_index);
#endif
}
