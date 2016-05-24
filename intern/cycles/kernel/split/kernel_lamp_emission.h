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

/* Note on kernel_lamp_emission
 * This is the 3rd kernel in the ray-tracing logic. This is the second of the
 * path-iteration kernels. This kernel takes care of the indirect lamp emission logic.
 * This kernel operates on QUEUE_ACTIVE_AND_REGENERATED_RAYS. It processes rays of state RAY_ACTIVE
 * and RAY_HIT_BACKGROUND.
 * We will empty QUEUE_ACTIVE_AND_REGENERATED_RAYS queue in this kernel.
 * The input/output of the kernel is as follows,
 * Throughput_coop ------------------------------------|--- kernel_lamp_emission --|--- PathRadiance_coop
 * Ray_coop -------------------------------------------|                           |--- Queue_data(QUEUE_ACTIVE_AND_REGENERATED_RAYS)
 * PathState_coop -------------------------------------|                           |--- Queue_index(QUEUE_ACTIVE_AND_REGENERATED_RAYS)
 * kg (globals) ---------------------------------------|                           |
 * Intersection_coop ----------------------------------|                           |
 * ray_state ------------------------------------------|                           |
 * Queue_data (QUEUE_ACTIVE_AND_REGENERATED_RAYS) -----|                           |
 * Queue_index (QUEUE_ACTIVE_AND_REGENERATED_RAYS) ----|                           |
 * queuesize ------------------------------------------|                           |
 * use_queues_flag ------------------------------------|                           |
 * sw -------------------------------------------------|                           |
 * sh -------------------------------------------------|                           |
 */
ccl_device void kernel_lamp_emission(
        KernelGlobals *kg,
        ccl_global float3 *throughput_coop,    /* Required for lamp emission */
        PathRadiance *PathRadiance_coop,       /* Required for lamp emission */
        ccl_global Ray *Ray_coop,              /* Required for lamp emission */
        ccl_global PathState *PathState_coop,  /* Required for lamp emission */
        Intersection *Intersection_coop,       /* Required for lamp emission */
        ccl_global char *ray_state,            /* Denotes the state of each ray */
        int sw, int sh,
        ccl_global char *use_queues_flag,      /* Used to decide if this kernel should use
                                                * queues to fetch ray index
                                                */
        int ray_index)
{
	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE) ||
	   IS_STATE(ray_state, ray_index, RAY_HIT_BACKGROUND))
	{
		PathRadiance *L = &PathRadiance_coop[ray_index];
		ccl_global PathState *state = &PathState_coop[ray_index];

		float3 throughput = throughput_coop[ray_index];
		Ray ray = Ray_coop[ray_index];

#ifdef __LAMP_MIS__
		if(kernel_data.integrator.use_lamp_mis && !(state->flag & PATH_RAY_CAMERA)) {
			/* ray starting from previous non-transparent bounce */
			Ray light_ray;

			light_ray.P = ray.P - state->ray_t*ray.D;
			state->ray_t += Intersection_coop[ray_index].t;
			light_ray.D = ray.D;
			light_ray.t = state->ray_t;
			light_ray.time = ray.time;
			light_ray.dD = ray.dD;
			light_ray.dP = ray.dP;
			/* intersect with lamp */
			float3 emission;

			if(indirect_lamp_emission(kg, kg->sd_input, state, &light_ray, &emission)) {
				path_radiance_accum_emission(L, throughput, emission, state->bounce);
			}
		}
#endif  /* __LAMP_MIS__ */
	}
}
