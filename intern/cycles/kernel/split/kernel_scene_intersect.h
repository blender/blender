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

/* Note on kernel_scene_intersect kernel.
 * This is the second kernel in the ray tracing logic. This is the first
 * of the path iteration kernels. This kernel takes care of scene_intersect function.
 *
 * This kernel changes the ray_state of RAY_REGENERATED rays to RAY_ACTIVE.
 * This kernel processes rays of ray state RAY_ACTIVE
 * This kernel determines the rays that have hit the background and changes their ray state to RAY_HIT_BACKGROUND.
 *
 * The input and output are as follows,
 *
 * Ray_coop ---------------------------------------|--------- kernel_scene_intersect----------|--- PathState
 * PathState_coop ---------------------------------|                                          |--- Intersection
 * ray_state --------------------------------------|                                          |--- ray_state
 * use_queues_flag --------------------------------|                                          |
 * QueueData(QUEUE_ACTIVE_AND_REGENERATED_RAYS) ---|                                          |
 * kg (globals) -----------------------------------|                                          |
 * rng_coop ---------------------------------------|                                          |
 * sw ---------------------------------------------|                                          |
 * sh ---------------------------------------------|                                          |
 * queuesize --------------------------------------|                                          |
 *
 * Note on Queues :
 * Ideally we would want kernel_scene_intersect to work on queues.
 * But during the very first time, the queues will be empty and hence we perform a direct mapping
 * between ray-index and thread-index; From the next time onward, the queue will be filled and
 * we may start operating on queues.
 *
 * State of queue during the first time this kernel is called :
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS and QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be empty.before and after this kernel
 *
 * State of queues during other times this kernel is called :
 * At entry,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS will have a mix of RAY_ACTIVE, RAY_UPDATE_BUFFER and RAY_REGENERATED rays;
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with RAY_TO_REGENERATE and RAY_UPDATE_BUFFER rays ;
 * (The rays that are in the state RAY_UPDATE_BUFFER in both the queues are actually the same rays; These
 * are the rays that were in RAY_ACTIVE state during the initial enqueue but on further processing
 * , by different kernels, have turned into RAY_UPDATE_BUFFER rays. Since all kernel, even after fetching from
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS, proceed further based on ray state information, RAY_UPDATE_BUFFER rays
 * being present in QUEUE_ACTIVE_AND_REGENERATED_RAYS does not cause any logical issues)
 * At exit,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS - All RAY_REGENERATED rays will have been converted to RAY_ACTIVE and
 * Some rays in QUEUE_ACTIVE_AND_REGENERATED_RAYS queue will move to state RAY_HIT_BACKGROUND
 * QUEUE_HITBF_BUFF_UPDATE_TOREGEN_RAYS - no change
 */

ccl_device void kernel_scene_intersect(
        KernelGlobals *kg,
        ccl_global uint *rng_coop,
        ccl_global Ray *Ray_coop,              /* Required for scene_intersect */
        ccl_global PathState *PathState_coop,  /* Required for scene_intersect */
        Intersection *Intersection_coop,       /* Required for scene_intersect */
        ccl_global char *ray_state,            /* Denotes the state of each ray */
        int sw, int sh,
        ccl_global char *use_queues_flag,      /* used to decide if this kernel should use
                                                * queues to fetch ray index */
#ifdef __KERNEL_DEBUG__
        DebugData *debugdata_coop,
#endif
        int ray_index)
{
	/* All regenerated rays become active here */
	if(IS_STATE(ray_state, ray_index, RAY_REGENERATED))
		ASSIGN_RAY_STATE(ray_state, ray_index, RAY_ACTIVE);

	if(!IS_STATE(ray_state, ray_index, RAY_ACTIVE))
		return;

#ifdef __KERNEL_DEBUG__
	DebugData *debug_data = &debugdata_coop[ray_index];
#endif
	Intersection *isect = &Intersection_coop[ray_index];
	PathState state = PathState_coop[ray_index];
	Ray ray = Ray_coop[ray_index];

	/* intersect scene */
	uint visibility = path_state_ray_visibility(kg, &state);

#ifdef __HAIR__
	float difl = 0.0f, extmax = 0.0f;
	uint lcg_state = 0;
	RNG rng = rng_coop[ray_index];

	if(kernel_data.bvh.have_curves) {
		if((kernel_data.cam.resolution == 1) && (state.flag & PATH_RAY_CAMERA)) {
			float3 pixdiff = ray.dD.dx + ray.dD.dy;
			/*pixdiff = pixdiff - dot(pixdiff, ray.D)*ray.D;*/
			difl = kernel_data.curve.minimum_width * len(pixdiff) * 0.5f;
		}

		extmax = kernel_data.curve.maximum_width;
		lcg_state = lcg_state_init(&rng, &state, 0x51633e2d);
	}

	bool hit = scene_intersect(kg, ray, visibility, isect, &lcg_state, difl, extmax);
#else
	bool hit = scene_intersect(kg, ray, visibility, isect, NULL, 0.0f, 0.0f);
#endif

#ifdef __KERNEL_DEBUG__
	if(state.flag & PATH_RAY_CAMERA) {
		debug_data->num_bvh_traversed_nodes += isect->num_traversed_nodes;
		debug_data->num_bvh_traversed_instances += isect->num_traversed_instances;
		debug_data->num_bvh_intersections += isect->num_intersections;
	}
	debug_data->num_ray_bounces++;
#endif

	if(!hit) {
		/* Change the state of rays that hit the background;
		 * These rays undergo special processing in the
		 * background_bufferUpdate kernel.
		 */
		ASSIGN_RAY_STATE(ray_state, ray_index, RAY_HIT_BACKGROUND);
	}
}
