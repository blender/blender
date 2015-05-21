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

#include "split/kernel_next_iteration_setup.h"

__kernel void kernel_ocl_path_trace_next_iteration_setup(
        ccl_global char *globals,
        ccl_constant KernelData *data,
        ccl_global char *shader_data,         /* Required for setting up ray for next iteration */
        ccl_global uint *rng_coop,            /* Required for setting up ray for next iteration */
        ccl_global float3 *throughput_coop,   /* Required for setting up ray for next iteration */
        PathRadiance *PathRadiance_coop,      /* Required for setting up ray for next iteration */
        ccl_global Ray *Ray_coop,             /* Required for setting up ray for next iteration */
        ccl_global PathState *PathState_coop, /* Required for setting up ray for next iteration */
        ccl_global Ray *LightRay_dl_coop,     /* Required for radiance update - direct lighting */
        ccl_global int *ISLamp_coop,          /* Required for radiance update - direct lighting */
        ccl_global BsdfEval *BSDFEval_coop,   /* Required for radiance update - direct lighting */
        ccl_global Ray *LightRay_ao_coop,     /* Required for radiance update - AO */
        ccl_global float3 *AOBSDF_coop,       /* Required for radiance update - AO */
        ccl_global float3 *AOAlpha_coop,      /* Required for radiance update - AO */
        ccl_global char *ray_state,           /* Denotes the state of each ray */
        ccl_global int *Queue_data,           /* Queue memory */
        ccl_global int *Queue_index,          /* Tracks the number of elements in each queue */
        int queuesize,                        /* Size (capacity) of each queue */
        ccl_global char *use_queues_flag)      /* flag to decide if scene_intersect kernel should use queues to fetch ray index */
{
	kernel_next_iteration_setup(globals,
	                            data,
	                            shader_data,
	                            rng_coop,
	                            throughput_coop,
	                            PathRadiance_coop,
	                            Ray_coop,
	                            PathState_coop,
	                            LightRay_dl_coop,
	                            ISLamp_coop,
	                            BSDFEval_coop,
	                            LightRay_ao_coop,
	                            AOBSDF_coop,
	                            AOAlpha_coop,
	                            ray_state,
	                            Queue_data,
	                            Queue_index,
	                            queuesize,
	                            use_queues_flag);
}
