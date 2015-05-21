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
        ccl_global char *globals,
        ccl_constant KernelData *data,
        ccl_global char *shader_data,          /* Required throughout the kernel except probabilistic path termination and AO */
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
	kernel_holdout_emission_blurring_pathtermination_ao(globals,
	                                                    data,
	                                                    shader_data,
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
	                                                    Queue_data,
	                                                    Queue_index,
	                                                    queuesize,
#ifdef __WORK_STEALING__
	                                                    start_sample,
#endif
	                                                    parallel_samples);
}
