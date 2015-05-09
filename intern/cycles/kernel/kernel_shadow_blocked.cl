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

#include "kernel_split.h"

/*
 * Note on kernel_ocl_path_trace_shadow_blocked kernel.
 * This is the ninth kernel in the ray tracing logic. This is the eighth
 * of the path iteration kernels. This kernel takes care of "shadow ray cast"
 * logic of the direct lighting and AO  part of ray tracing.
 *
 * The input and output are as follows,
 *
 * PathState_coop ----------------------------------|--- kernel_ocl_path_trace_shadow_blocked --|
 * LightRay_dl_coop --------------------------------|                                           |--- LightRay_dl_coop
 * LightRay_ao_coop --------------------------------|                                           |--- LightRay_ao_coop
 * ray_state ---------------------------------------|                                           |--- ray_state
 * Queue_data(QUEUE_SHADOW_RAY_CAST_AO_RAYS &       |                                           |--- Queue_data (QUEUE_SHADOW_RAY_CAST_AO_RAYS & QUEUE_SHADOW_RAY_CAST_AO_RAYS)
	      QUEUE_SHADOW_RAY_CAST_DL_RAYS) -------|                                           |
 * Queue_index(QUEUE_SHADOW_RAY_CAST_AO_RAYS&
	      QUEUE_SHADOW_RAY_CAST_DL_RAYS) -------|                                           |
 * kg (globals + data) -----------------------------|                                           |
 * queuesize ---------------------------------------|                                           |
 *
 * Note on shader_shadow : shader_shadow is neither input nor output to this kernel. shader_shadow is filled and consumed in this kernel itself.
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

__kernel void kernel_ocl_path_trace_shadow_blocked_direct_lighting(
	ccl_global char *globals,
	ccl_constant KernelData *data,
	ccl_global char *shader_shadow,             /* Required for shadow blocked */
	ccl_global PathState *PathState_coop,       /* Required for shadow blocked */
	ccl_global Ray *LightRay_dl_coop,           /* Required for direct lighting's shadow blocked */
	ccl_global Ray *LightRay_ao_coop,           /* Required for AO's shadow blocked */
	Intersection *Intersection_coop_AO,
	Intersection *Intersection_coop_DL,
	ccl_global char *ray_state,
	ccl_global int *Queue_data,                 /* Queue memory */
	ccl_global int *Queue_index,                /* Tracks the number of elements in each queue */
	int queuesize,                              /* Size (capacity) of each queue */
	int total_num_rays
	)
{
#if 0
	/* we will make the Queue_index entries '0' in the next kernel */
	if(get_global_id(0) == 0 && get_global_id(1) == 0) {
		/* We empty this queue here */
		Queue_index[QUEUE_SHADOW_RAY_CAST_AO_RAYS] = 0;
		Queue_index[QUEUE_SHADOW_RAY_CAST_DL_RAYS] = 0;
	}
#endif

	int lidx = get_local_id(1) * get_local_id(0) + get_local_id(0);

	ccl_local unsigned int ao_queue_length;
	ccl_local unsigned int dl_queue_length;
	if(lidx == 0) {
		ao_queue_length = Queue_index[QUEUE_SHADOW_RAY_CAST_AO_RAYS];
		dl_queue_length = Queue_index[QUEUE_SHADOW_RAY_CAST_DL_RAYS];
	}
	barrier(CLK_LOCAL_MEM_FENCE);

	/* flag determining if the current ray is to process shadow ray for AO or DL */
	char shadow_blocked_type = -1;
	/* flag determining if we need to update L */
	char update_path_radiance = 0;

	int ray_index = QUEUE_EMPTY_SLOT;
	int thread_index = get_global_id(1) * get_global_size(0) + get_global_id(0);
	if(thread_index < ao_queue_length + dl_queue_length) {
		if(thread_index < ao_queue_length) {
			ray_index = get_ray_index(thread_index, QUEUE_SHADOW_RAY_CAST_AO_RAYS, Queue_data, queuesize, 1);
			shadow_blocked_type = RAY_SHADOW_RAY_CAST_AO;
		} else {
			ray_index = get_ray_index(thread_index - ao_queue_length, QUEUE_SHADOW_RAY_CAST_DL_RAYS, Queue_data, queuesize, 1);
			shadow_blocked_type = RAY_SHADOW_RAY_CAST_DL;
		}
	}

	if(ray_index == QUEUE_EMPTY_SLOT)
		return;

	if(IS_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_DL) || IS_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_AO)) {
		/* Load kernel global structure */
		KernelGlobals *kg = (KernelGlobals *)globals;
		ShaderData *sd_shadow  = (ShaderData *)shader_shadow;

		ccl_global PathState *state = &PathState_coop[ray_index];
		ccl_global Ray *light_ray_dl_global = &LightRay_dl_coop[ray_index];
		ccl_global Ray *light_ray_ao_global = &LightRay_ao_coop[ray_index];
		Intersection *isect_ao_global = &Intersection_coop_AO[ray_index];
		Intersection *isect_dl_global = &Intersection_coop_DL[ray_index];

		ccl_global Ray *light_ray_global = shadow_blocked_type == RAY_SHADOW_RAY_CAST_AO ? light_ray_ao_global : light_ray_dl_global;
		Intersection *isect_global = RAY_SHADOW_RAY_CAST_AO ? isect_ao_global : isect_dl_global;

		float3 shadow;
		update_path_radiance = !(shadow_blocked(kg, state, light_ray_global, &shadow, sd_shadow, isect_global));

		/* We use light_ray_global's P and t to store shadow and update_path_radiance */
		light_ray_global->P = shadow;
		light_ray_global->t = update_path_radiance;
	}
}
