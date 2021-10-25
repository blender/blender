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

/* This kernel takes care of the logic to process "material of type holdout",
 * indirect primitive emission, bsdf blurring, probabilistic path termination
 * and AO.
 *
 * This kernels determines the rays for which a shadow_blocked() function
 * associated with AO should be executed. Those rays for which a
 * shadow_blocked() function for AO must be executed are marked with flag
 * RAY_SHADOW_RAY_CAST_ao and enqueued into the queue
 * QUEUE_SHADOW_RAY_CAST_AO_RAYS
 *
 * Ray state of rays that are terminated in this kernel are changed to RAY_UPDATE_BUFFER
 *
 * Note on Queues:
 * This kernel fetches rays from the queue QUEUE_ACTIVE_AND_REGENERATED_RAYS
 * and processes only the rays of state RAY_ACTIVE.
 * There are different points in this kernel where a ray may terminate and
 * reach RAY_UPDATE_BUFFER state. These rays are enqueued into
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue. These rays will still be present
 * in QUEUE_ACTIVE_AND_REGENERATED_RAYS queue, but since their ray-state has
 * been changed to RAY_UPDATE_BUFFER, there is no problem.
 *
 * State of queues when this kernel is called:
 * At entry,
 *   - QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE and
 *     RAY_REGENERATED rays
 *   - QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with
 *     RAY_TO_REGENERATE rays.
 *   - QUEUE_SHADOW_RAY_CAST_AO_RAYS will be empty.
 * At exit,
 *   - QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE,
 *     RAY_REGENERATED and RAY_UPDATE_BUFFER rays.
 *   - QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with
 *     RAY_TO_REGENERATE and RAY_UPDATE_BUFFER rays.
 *   - QUEUE_SHADOW_RAY_CAST_AO_RAYS will be filled with rays marked with
 *     flag RAY_SHADOW_RAY_CAST_AO
 */

ccl_device void kernel_holdout_emission_blurring_pathtermination_ao(
        KernelGlobals *kg,
        ccl_local_param BackgroundAOLocals *locals)
{
	if(ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
		locals->queue_atomics_bg = 0;
		locals->queue_atomics_ao = 0;
	}
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

#ifdef __AO__
	char enqueue_flag = 0;
#endif
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
#endif  /* __COMPUTE_DEVICE_GPU__ */

#ifndef __COMPUTE_DEVICE_GPU__
	if(ray_index != QUEUE_EMPTY_SLOT) {
#endif

	int stride = kernel_split_params.stride;

	unsigned int work_index;
	unsigned int pixel_x;
	unsigned int pixel_y;

	unsigned int tile_x;
	unsigned int tile_y;
	unsigned int sample;

	RNG rng = kernel_split_state.rng[ray_index];
	ccl_global PathState *state = 0x0;
	float3 throughput;

	ccl_global char *ray_state = kernel_split_state.ray_state;
	ShaderData *sd = &kernel_split_state.sd[ray_index];
	ccl_global float *buffer = kernel_split_params.buffer;

	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {

		throughput = kernel_split_state.throughput[ray_index];
		state = &kernel_split_state.path_state[ray_index];

		work_index = kernel_split_state.work_array[ray_index];
		sample = get_work_sample(kg, work_index, ray_index) + kernel_split_params.start_sample;
		get_work_pixel_tile_position(kg, &pixel_x, &pixel_y,
		                        &tile_x, &tile_y,
		                        work_index,
		                        ray_index);

		buffer += (kernel_split_params.offset + pixel_x + pixel_y * stride) * kernel_data.film.pass_stride;

#ifdef __SHADOW_TRICKS__
		if((sd->object_flag & SD_OBJECT_SHADOW_CATCHER)) {
			if(state->flag & PATH_RAY_CAMERA) {
				PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
				state->flag |= (PATH_RAY_SHADOW_CATCHER |
				                PATH_RAY_SHADOW_CATCHER_ONLY |
				                PATH_RAY_STORE_SHADOW_INFO);
				state->catcher_object = sd->object;
				if(!kernel_data.background.transparent) {
					ccl_global Ray *ray = &kernel_split_state.ray[ray_index];
					L->shadow_background_color = indirect_background(
					        kg,
					        &kernel_split_state.sd_DL_shadow[ray_index],
					        state,
					        ray);
				}
				L->shadow_radiance_sum = path_radiance_clamp_and_sum(kg, L);
				L->shadow_throughput = average(throughput);
			}
		}
		else {
			state->flag &= ~PATH_RAY_SHADOW_CATCHER_ONLY;
		}
#endif  /* __SHADOW_TRICKS__ */

		/* holdout */
#ifdef __HOLDOUT__
		if(((sd->flag & SD_HOLDOUT) ||
		    (sd->object_flag & SD_OBJECT_HOLDOUT_MASK)) &&
		   (state->flag & PATH_RAY_CAMERA))
		{
			if(kernel_data.background.transparent) {
				float3 holdout_weight;
				if(sd->object_flag & SD_OBJECT_HOLDOUT_MASK) {
					holdout_weight = make_float3(1.0f, 1.0f, 1.0f);
				}
				else {
					holdout_weight = shader_holdout_eval(kg, sd);
				}
				/* any throughput is ok, should all be identical here */
				kernel_split_state.L_transparent[ray_index] += average(holdout_weight*throughput);
			}
			if(sd->object_flag & SD_OBJECT_HOLDOUT_MASK) {
				kernel_split_path_end(kg, ray_index);
			}
		}
#endif  /* __HOLDOUT__ */
	}

	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
		PathRadiance *L = &kernel_split_state.path_radiance[ray_index];

#ifdef __BRANCHED_PATH__
		if(!IS_FLAG(ray_state, ray_index, RAY_BRANCHED_INDIRECT))
#endif  /* __BRANCHED_PATH__ */
		{
			/* Holdout mask objects do not write data passes. */
			kernel_write_data_passes(kg,
				                     buffer,
				                     L,
				                     sd,
				                     sample,
				                     state,
				                     throughput);
		}

		/* Blurring of bsdf after bounces, for rays that have a small likelihood
		 * of following this particular path (diffuse, rough glossy.
		 */
#ifndef __BRANCHED_PATH__
		if(kernel_data.integrator.filter_glossy != FLT_MAX)
#else
		if(kernel_data.integrator.filter_glossy != FLT_MAX &&
		   (!kernel_data.integrator.branched || IS_FLAG(ray_state, ray_index, RAY_BRANCHED_INDIRECT)))
#endif  /* __BRANCHED_PATH__ */
		{
			float blur_pdf = kernel_data.integrator.filter_glossy*state->min_ray_pdf;
			if(blur_pdf < 1.0f) {
				float blur_roughness = sqrtf(1.0f - blur_pdf)*0.5f;
				shader_bsdf_blur(kg, sd, blur_roughness);
			}
		}

#ifdef __EMISSION__
		/* emission */
		if(sd->flag & SD_EMISSION) {
			/* TODO(sergey): is isect.t wrong here for transparent surfaces? */
			float3 emission = indirect_primitive_emission(
			        kg,
			        sd,
			        kernel_split_state.isect[ray_index].t,
			        state->flag,
			        state->ray_pdf);
			path_radiance_accum_emission(L, throughput, emission, state->bounce);
		}
#endif  /* __EMISSION__ */

		/* Path termination. this is a strange place to put the termination, it's
		 * mainly due to the mixed in MIS that we use. gives too many unneeded
		 * shader evaluations, only need emission if we are going to terminate.
		 */
#ifndef __BRANCHED_PATH__
		float probability = path_state_terminate_probability(kg, state, throughput);
#else
		float probability = 1.0f;

		if(!kernel_data.integrator.branched) {
			probability = path_state_terminate_probability(kg, state, throughput);
		}
		else if(IS_FLAG(ray_state, ray_index, RAY_BRANCHED_INDIRECT)) {
			int num_samples = kernel_split_state.branched_state[ray_index].num_samples;
			probability = path_state_terminate_probability(kg, state, throughput*num_samples);
		}
		else if(state->flag & PATH_RAY_TRANSPARENT) {
			probability = path_state_terminate_probability(kg, state, throughput);
		}
#endif

		if(probability == 0.0f) {
			kernel_split_path_end(kg, ray_index);
		}

		if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
			if(probability != 1.0f) {
				float terminate = path_state_rng_1D_for_decision(kg, &rng, state, PRNG_TERMINATE);
				if(terminate >= probability) {
					kernel_split_path_end(kg, ray_index);
				}
				else {
					kernel_split_state.throughput[ray_index] = throughput/probability;
				}
			}

			kernel_update_denoising_features(kg, sd, state, L);
		}
	}

#ifdef __AO__
	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
		/* ambient occlusion */
		if(kernel_data.integrator.use_ambient_occlusion || (sd->flag & SD_AO)) {
			enqueue_flag = 1;
		}
	}
#endif  /* __AO__ */

	kernel_split_state.rng[ray_index] = rng;

#ifndef __COMPUTE_DEVICE_GPU__
	}
#endif

#ifdef __AO__
	/* Enqueue to-shadow-ray-cast rays. */
	enqueue_ray_index_local(ray_index,
	                        QUEUE_SHADOW_RAY_CAST_AO_RAYS,
	                        enqueue_flag,
	                        kernel_split_params.queue_size,
	                        &locals->queue_atomics_ao,
	                        kernel_split_state.queue_data,
	                        kernel_split_params.queue_index);
#endif
}

CCL_NAMESPACE_END
