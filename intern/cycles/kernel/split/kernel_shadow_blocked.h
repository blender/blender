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

#include "kernel_split_common.h"

/* Note on kernel_shadow_blocked kernel.
 * This is the ninth kernel in the ray tracing logic. This is the eighth
 * of the path iteration kernels. This kernel takes care of "shadow ray cast"
 * logic of the direct lighting and AO  part of ray tracing.
 *
 * The input and output are as follows,
 *
 * PathState_coop ----------------------------------|--- kernel_shadow_blocked --|
 * LightRay_dl_coop --------------------------------|                            |--- LightRay_dl_coop
 * LightRay_ao_coop --------------------------------|                            |--- LightRay_ao_coop
 * ray_state ---------------------------------------|                            |--- ray_state
 * Queue_data(QUEUE_SHADOW_RAY_CAST_AO_RAYS &       |                            |--- Queue_data (QUEUE_SHADOW_RAY_CAST_AO_RAYS & QUEUE_SHADOW_RAY_CAST_AO_RAYS)
              QUEUE_SHADOW_RAY_CAST_DL_RAYS) -------|                            |
 * Queue_index(QUEUE_SHADOW_RAY_CAST_AO_RAYS&
              QUEUE_SHADOW_RAY_CAST_DL_RAYS) -------|                            |
 * kg (globals) ------------------------------------|                            |
 * queuesize ---------------------------------------|                            |
 *
 * Note on sd_shadow : sd_shadow is neither input nor output to this kernel. sd_shadow is filled and consumed in this kernel itself.
 * Note on queues :
 * The kernel fetches from QUEUE_SHADOW_RAY_CAST_AO_RAYS and QUEUE_SHADOW_RAY_CAST_DL_RAYS queues. We will empty
 * these queues this kernel.
 * State of queues when this kernel is called :
 * state of queues QUEUE_ACTIVE_AND_REGENERATED_RAYS and QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be same
 * before and after this kernel call.
 * QUEUE_SHADOW_RAY_CAST_AO_RAYS & QUEUE_SHADOW_RAY_CAST_DL_RAYS will be filled with rays marked with flags RAY_SHADOW_RAY_CAST_AO
 * and RAY_SHADOW_RAY_CAST_DL respectively, during kernel entry.
 * QUEUE_SHADOW_RAY_CAST_AO_RAYS and QUEUE_SHADOW_RAY_CAST_DL_RAYS will be empty at kernel exit.
 */
ccl_device void kernel_shadow_blocked(
        KernelGlobals *kg,
        ccl_global PathState *PathState_coop,  /* Required for shadow blocked */
        ccl_global Ray *LightRay_dl_coop,      /* Required for direct lighting's shadow blocked */
        ccl_global Ray *LightRay_ao_coop,      /* Required for AO's shadow blocked */
        ccl_global char *ray_state,
        char shadow_blocked_type,
        int ray_index)
{
	/* Flag determining if we need to update L. */
	char update_path_radiance = 0;

	if(IS_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_DL) ||
	   IS_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_AO))
	{
		ccl_global PathState *state = &PathState_coop[ray_index];
		ccl_global Ray *light_ray_dl_global = &LightRay_dl_coop[ray_index];
		ccl_global Ray *light_ray_ao_global = &LightRay_ao_coop[ray_index];

		ccl_global Ray *light_ray_global =
		        shadow_blocked_type == RAY_SHADOW_RAY_CAST_AO
		                ? light_ray_ao_global
		                : light_ray_dl_global;

		float3 shadow;
		update_path_radiance = !(shadow_blocked(kg,
		                                        kg->sd_input,
		                                        state,
		                                        light_ray_global,
		                                        &shadow));

		/* We use light_ray_global's P and t to store shadow and
		 * update_path_radiance.
		 */
		light_ray_global->P = shadow;
		light_ray_global->t = update_path_radiance;
	}
}
