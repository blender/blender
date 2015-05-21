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
        ccl_global char *globals,
        ccl_constant KernelData *data,
        ccl_global char *shader_data,           /* Required for direct lighting */
        ccl_global char *shader_DL,             /* Required for direct lighting */
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
	kernel_direct_lighting(globals,
	                       data,
	                       shader_data,
	                       shader_DL,
	                       rng_coop,
	                       PathState_coop,
	                       ISLamp_coop,
	                       LightRay_coop,
	                       BSDFEval_coop,
	                       ray_state,
	                       Queue_data,
	                       Queue_index,
	                       queuesize);
}
