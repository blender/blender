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

#include "split/kernel_shadow_blocked.h"

__kernel void kernel_ocl_path_trace_shadow_blocked(
        ccl_global char *globals,
        ccl_constant KernelData *data,
        ccl_global char *shader_shadow,        /* Required for shadow blocked */
        ccl_global PathState *PathState_coop,  /* Required for shadow blocked */
        ccl_global Ray *LightRay_dl_coop,      /* Required for direct lighting's shadow blocked */
        ccl_global Ray *LightRay_ao_coop,      /* Required for AO's shadow blocked */
        Intersection *Intersection_coop_AO,
        Intersection *Intersection_coop_DL,
        ccl_global char *ray_state,
        ccl_global int *Queue_data,            /* Queue memory */
        ccl_global int *Queue_index,           /* Tracks the number of elements in each queue */
        int queuesize,                         /* Size (capacity) of each queue */
        int total_num_rays)
{
	kernel_shadow_blocked(globals,
	                      data,
	                      shader_shadow,
	                      PathState_coop,
	                      LightRay_dl_coop,
	                      LightRay_ao_coop,
	                      Intersection_coop_AO,
	                      Intersection_coop_DL,
	                      ray_state,
	                      Queue_data,
	                      Queue_index,
	                      queuesize,
	                      total_num_rays);
}
