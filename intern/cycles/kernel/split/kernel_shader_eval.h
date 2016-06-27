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

/* Note on kernel_shader_eval kernel
 * This kernel is the 5th kernel in the ray tracing logic. This is
 * the 4rd kernel in path iteration. This kernel sets up the ShaderData
 * structure from the values computed by the previous kernels. It also identifies
 * the rays of state RAY_TO_REGENERATE and enqueues them in QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue.
 *
 * The input and output of the kernel is as follows,
 * rng_coop -------------------------------------------|--- kernel_shader_eval --|--- sd
 * Ray_coop -------------------------------------------|                         |--- Queue_data (QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS)
 * PathState_coop -------------------------------------|                         |--- Queue_index (QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS)
 * Intersection_coop ----------------------------------|                         |
 * Queue_data (QUEUE_ACTIVE_AND_REGENERATD_RAYS)-------|                         |
 * Queue_index(QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS)---|                         |
 * ray_state ------------------------------------------|                         |
 * kg (globals) ---------------------------------------|                         |
 * queuesize ------------------------------------------|                         |
 *
 * Note on Queues :
 * This kernel reads from the QUEUE_ACTIVE_AND_REGENERATED_RAYS queue and processes
 * only the rays of state RAY_ACTIVE;
 * State of queues when this kernel is called,
 * at entry,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE and RAY_REGENERATED rays
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be empty.
 * at exit,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE and RAY_REGENERATED rays
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with RAY_TO_REGENERATE rays
 */
ccl_device void kernel_shader_eval(
        KernelGlobals *kg,
        ShaderData *sd,                        /* Output ShaderData structure to be filled */
        ccl_global uint *rng_coop,             /* Required for rbsdf calculation */
        ccl_global Ray *Ray_coop,              /* Required for setting up shader from ray */
        ccl_global PathState *PathState_coop,  /* Required for all functions in this kernel */
        Intersection *Intersection_coop,       /* Required for setting up shader from ray */
        ccl_global char *ray_state,            /* Denotes the state of each ray */
        int ray_index)
{
	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
		Intersection *isect = &Intersection_coop[ray_index];
		ccl_global uint *rng = &rng_coop[ray_index];
		ccl_global PathState *state = &PathState_coop[ray_index];
		Ray ray = Ray_coop[ray_index];

		shader_setup_from_ray(kg,
		                      sd,
		                      isect,
		                      &ray);
		float rbsdf = path_state_rng_1D_for_decision(kg, rng, state, PRNG_BSDF);
		shader_eval_surface(kg, sd, rng, state, rbsdf, state->flag, SHADER_CONTEXT_MAIN);
	}
}
