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

CCL_NAMESPACE_BEGIN

/* Note on kernel_setup_next_iteration kernel.
 * This is the tenth kernel in the ray tracing logic. This is the ninth
 * of the path iteration kernels. This kernel takes care of setting up
 * Ray for the next iteration of path-iteration and accumulating radiance
 * corresponding to AO and direct-lighting
 *
 * Ray state of rays that are terminated in this kernel are changed to RAY_UPDATE_BUFFER
 *
 * The input and output are as follows,
 *
 * rng_coop ---------------------------------------------|--- kernel_next_iteration_setup -|--- Queue_index (QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS)
 * throughput_coop --------------------------------------|                                 |--- Queue_data (QUEUE_HITBF_BUFF_UPDATE_TOREGEN_RAYS)
 * PathRadiance_coop ------------------------------------|                                 |--- throughput_coop
 * PathState_coop ---------------------------------------|                                 |--- PathRadiance_coop
 * sd ---------------------------------------------------|                                 |--- PathState_coop
 * ray_state --------------------------------------------|                                 |--- ray_state
 * Queue_data (QUEUE_ACTIVE_AND_REGENERATD_RAYS) --------|                                 |--- Ray_coop
 * Queue_index (QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS) ---|                                 |--- use_queues_flag
 * Ray_coop ---------------------------------------------|                                 |
 * kg (globals) -----------------------------------------|                                 |
 * LightRay_dl_coop -------------------------------------|
 * ISLamp_coop ------------------------------------------|
 * BSDFEval_coop ----------------------------------------|
 * LightRay_ao_coop -------------------------------------|
 * AOBSDF_coop ------------------------------------------|
 * AOAlpha_coop -----------------------------------------|
 *
 * Note on queues,
 * This kernel fetches rays from the queue QUEUE_ACTIVE_AND_REGENERATED_RAYS and processes only
 * the rays of state RAY_ACTIVE.
 * There are different points in this kernel where a ray may terminate and reach RAY_UPDATE_BUFF
 * state. These rays are enqueued into QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue. These rays will
 * still be present in QUEUE_ACTIVE_AND_REGENERATED_RAYS queue, but since their ray-state has been
 * changed to RAY_UPDATE_BUFF, there is no problem.
 *
 * State of queues when this kernel is called :
 * At entry,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE, RAY_REGENERATED, RAY_UPDATE_BUFFER rays.
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with RAY_TO_REGENERATE and RAY_UPDATE_BUFFER rays
 * At exit,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE, RAY_REGENERATED and more RAY_UPDATE_BUFFER rays.
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with RAY_TO_REGENERATE and more RAY_UPDATE_BUFFER rays
 */
ccl_device void kernel_next_iteration_setup(KernelGlobals *kg)
{
	ccl_local unsigned int local_queue_atomics;
	if(ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
		local_queue_atomics = 0;
	}
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

	if(ccl_global_id(0) == 0 && ccl_global_id(1) == 0) {
		/* If we are here, then it means that scene-intersect kernel
		* has already been executed atleast once. From the next time,
		* scene-intersect kernel may operate on queues to fetch ray index
		*/
		*kernel_split_params.use_queues_flag = 1;

		/* Mark queue indices of QUEUE_SHADOW_RAY_CAST_AO_RAYS and
		 * QUEUE_SHADOW_RAY_CAST_DL_RAYS queues that were made empty during the
		 * previous kernel.
		 */
		kernel_split_params.queue_index[QUEUE_SHADOW_RAY_CAST_AO_RAYS] = 0;
		kernel_split_params.queue_index[QUEUE_SHADOW_RAY_CAST_DL_RAYS] = 0;
	}

	char enqueue_flag = 0;
	int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	ray_index = get_ray_index(kg, ray_index,
	                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
	                          kernel_split_state.queue_data,
	                          kernel_split_params.queue_size,
	                          0);

#ifdef __COMPUTE_DEVICE_GPU__
	/* If we are executing on a GPU device, we exit all threads that are not
	 * required.
	 *
	 * If we are executing on a CPU device, then we need to keep all threads
	 * active since we have barrier() calls later in the kernel. CPU devices,
	 * expect all threads to execute barrier statement.
	 */
	if(ray_index == QUEUE_EMPTY_SLOT) {
		return;
	}
#endif

#ifndef __COMPUTE_DEVICE_GPU__
	if(ray_index != QUEUE_EMPTY_SLOT) {
#endif

	/* Load ShaderData structure. */
	PathRadiance *L = NULL;
	ccl_global PathState *state = NULL;
	ccl_global char *ray_state = kernel_split_state.ray_state;

	/* Path radiance update for AO/Direct_lighting's shadow blocked. */
	if(IS_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_DL) ||
	   IS_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_AO))
	{
		state = &kernel_split_state.path_state[ray_index];
		L = &kernel_split_state.path_radiance[ray_index];
		float3 _throughput = kernel_split_state.throughput[ray_index];

		if(IS_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_AO)) {
			float3 shadow = kernel_split_state.ao_light_ray[ray_index].P;
			// TODO(mai): investigate correctness here
			char update_path_radiance = (char)kernel_split_state.ao_light_ray[ray_index].t;
			if(update_path_radiance) {
				path_radiance_accum_ao(L,
				                       _throughput,
				                       kernel_split_state.ao_alpha[ray_index],
				                       kernel_split_state.ao_bsdf[ray_index],
				                       shadow,
				                       state->bounce);
			}
			REMOVE_RAY_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_AO);
		}

		if(IS_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_DL)) {
			float3 shadow = kernel_split_state.light_ray[ray_index].P;
			// TODO(mai): investigate correctness here
			char update_path_radiance = (char)kernel_split_state.light_ray[ray_index].t;
			if(update_path_radiance) {
				BsdfEval L_light = kernel_split_state.bsdf_eval[ray_index];
				path_radiance_accum_light(L,
				                          _throughput,
				                          &L_light,
				                          shadow,
				                          1.0f,
				                          state->bounce,
				                          kernel_split_state.is_lamp[ray_index]);
			}
			REMOVE_RAY_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_DL);
		}
	}

	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
		ccl_global float3 *throughput = &kernel_split_state.throughput[ray_index];
		ccl_global Ray *ray = &kernel_split_state.ray[ray_index];
		ccl_global RNG *rng = &kernel_split_state.rng[ray_index];
		state = &kernel_split_state.path_state[ray_index];
		L = &kernel_split_state.path_radiance[ray_index];

		/* Compute direct lighting and next bounce. */
		if(!kernel_path_surface_bounce(kg, rng, &kernel_split_state.sd[ray_index], throughput, state, L, ray)) {
			ASSIGN_RAY_STATE(ray_state, ray_index, RAY_UPDATE_BUFFER);
			enqueue_flag = 1;
		}
	}

#ifndef __COMPUTE_DEVICE_GPU__
	}
#endif

	/* Enqueue RAY_UPDATE_BUFFER rays. */
	enqueue_ray_index_local(ray_index,
	                        QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	                        enqueue_flag,
	                        kernel_split_params.queue_size,
	                        &local_queue_atomics,
	                        kernel_split_state.queue_data,
	                        kernel_split_params.queue_index);
}

CCL_NAMESPACE_END

