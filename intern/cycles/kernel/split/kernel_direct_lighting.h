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

/* Note on kernel_direct_lighting kernel.
 * This is the eighth kernel in the ray tracing logic. This is the seventh
 * of the path iteration kernels. This kernel takes care of direct lighting
 * logic. However, the "shadow ray cast" part of direct lighting is handled
 * in the next kernel.
 *
 * This kernels determines the rays for which a shadow_blocked() function associated with direct lighting should be executed.
 * Those rays for which a shadow_blocked() function for direct-lighting must be executed, are marked with flag RAY_SHADOW_RAY_CAST_DL and
 * enqueued into the queue QUEUE_SHADOW_RAY_CAST_DL_RAYS
 *
 * The input and output are as follows,
 *
 * rng_coop -----------------------------------------|--- kernel_direct_lighting --|--- BSDFEval_coop
 * PathState_coop -----------------------------------|                             |--- ISLamp_coop
 * sd -----------------------------------------------|                             |--- LightRay_coop
 * ray_state ----------------------------------------|                             |--- ray_state
 * Queue_data (QUEUE_ACTIVE_AND_REGENERATED_RAYS) ---|                             |
 * kg (globals) -------------------------------------|                             |
 * queuesize ----------------------------------------|                             |
 *
 * Note on Queues :
 * This kernel only reads from the QUEUE_ACTIVE_AND_REGENERATED_RAYS queue and processes
 * only the rays of state RAY_ACTIVE; If a ray needs to execute the corresponding shadow_blocked
 * part, after direct lighting, the ray is marked with RAY_SHADOW_RAY_CAST_DL flag.
 *
 * State of queues when this kernel is called :
 * state of queues QUEUE_ACTIVE_AND_REGENERATED_RAYS and QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be same
 * before and after this kernel call.
 * QUEUE_SHADOW_RAY_CAST_DL_RAYS queue will be filled with rays for which a shadow_blocked function must be executed, after this
 * kernel call. Before this kernel call the QUEUE_SHADOW_RAY_CAST_DL_RAYS will be empty.
 */
ccl_device char kernel_direct_lighting(
        KernelGlobals *kg,
        ShaderData *sd,                         /* Required for direct lighting */
        ccl_global uint *rng_coop,              /* Required for direct lighting */
        ccl_global PathState *PathState_coop,   /* Required for direct lighting */
        ccl_global int *ISLamp_coop,            /* Required for direct lighting */
        ccl_global Ray *LightRay_coop,          /* Required for direct lighting */
        ccl_global BsdfEval *BSDFEval_coop,     /* Required for direct lighting */
        ccl_global char *ray_state,             /* Denotes the state of each ray */
        int ray_index)
{
	char enqueue_flag = 0;
	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
		ccl_global PathState *state = &PathState_coop[ray_index];

		/* direct lighting */
#ifdef __EMISSION__
		if((kernel_data.integrator.use_direct_light &&
		    (ccl_fetch(sd, flag) & SD_BSDF_HAS_EVAL)))
		{
			/* Sample illumination from lights to find path contribution. */
			ccl_global RNG* rng = &rng_coop[ray_index];
			float light_t = path_state_rng_1D(kg, rng, state, PRNG_LIGHT);
			float light_u, light_v;
			path_state_rng_2D(kg, rng, state, PRNG_LIGHT_U, &light_u, &light_v);
			float terminate = path_state_rng_light_termination(kg, rng, state);

			LightSample ls;
			if(light_sample(kg,
			                light_t, light_u, light_v,
			                ccl_fetch(sd, time),
			                ccl_fetch(sd, P),
			                state->bounce,
			                &ls)) {

				Ray light_ray;
#ifdef __OBJECT_MOTION__
				light_ray.time = ccl_fetch(sd, time);
#endif

				BsdfEval L_light;
				bool is_lamp;
				if(direct_emission(kg, sd, kg->sd_input, &ls, state, &light_ray, &L_light, &is_lamp, terminate)) {
					/* Write intermediate data to global memory to access from
					 * the next kernel.
					 */
					LightRay_coop[ray_index] = light_ray;
					BSDFEval_coop[ray_index] = L_light;
					ISLamp_coop[ray_index] = is_lamp;
					/* Mark ray state for next shadow kernel. */
					ADD_RAY_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_DL);
					enqueue_flag = 1;
				}
			}
		}
#endif  /* __EMISSION__ */
	}
	return enqueue_flag;
}
