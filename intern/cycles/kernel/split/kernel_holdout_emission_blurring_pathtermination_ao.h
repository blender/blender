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

/* Note on kernel_holdout_emission_blurring_pathtermination_ao kernel.
 * This is the sixth kernel in the ray tracing logic. This is the fifth
 * of the path iteration kernels. This kernel takes care of the logic to process
 * "material of type holdout", indirect primitive emission, bsdf blurring,
 * probabilistic path termination and AO.
 *
 * This kernels determines the rays for which a shadow_blocked() function associated with AO should be executed.
 * Those rays for which a shadow_blocked() function for AO must be executed are marked with flag RAY_SHADOW_RAY_CAST_ao and
 * enqueued into the queue QUEUE_SHADOW_RAY_CAST_AO_RAYS
 *
 * Ray state of rays that are terminated in this kernel are changed to RAY_UPDATE_BUFFER
 *
 * The input and output are as follows,
 *
 * rng_coop ---------------------------------------------|--- kernel_holdout_emission_blurring_pathtermination_ao ---|--- Queue_index (QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS)
 * throughput_coop --------------------------------------|                                                           |--- PathState_coop
 * PathRadiance_coop ------------------------------------|                                                           |--- throughput_coop
 * Intersection_coop ------------------------------------|                                                           |--- L_transparent_coop
 * PathState_coop ---------------------------------------|                                                           |--- per_sample_output_buffers
 * L_transparent_coop -----------------------------------|                                                           |--- PathRadiance_coop
 * sd ---------------------------------------------------|                                                           |--- ShaderData
 * ray_state --------------------------------------------|                                                           |--- ray_state
 * Queue_data (QUEUE_ACTIVE_AND_REGENERATED_RAYS) -------|                                                           |--- Queue_data (QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS)
 * Queue_index (QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS) ---|                                                           |--- AOAlpha_coop
 * kg (globals) -----------------------------------------|                                                           |--- AOBSDF_coop
 * parallel_samples -------------------------------------|                                                           |--- AOLightRay_coop
 * per_sample_output_buffers ----------------------------|                                                           |
 * sw ---------------------------------------------------|                                                           |
 * sh ---------------------------------------------------|                                                           |
 * sx ---------------------------------------------------|                                                           |
 * sy ---------------------------------------------------|                                                           |
 * stride -----------------------------------------------|                                                           |
 * work_array -------------------------------------------|                                                           |
 * queuesize --------------------------------------------|                                                           |
 * start_sample -----------------------------------------|                                                           |
 *
 * Note on Queues :
 * This kernel fetches rays from the queue QUEUE_ACTIVE_AND_REGENERATED_RAYS and processes only
 * the rays of state RAY_ACTIVE.
 * There are different points in this kernel where a ray may terminate and reach RAY_UPDATE_BUFFER
 * state. These rays are enqueued into QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS queue. These rays will
 * still be present in QUEUE_ACTIVE_AND_REGENERATED_RAYS queue, but since their ray-state has been
 * changed to RAY_UPDATE_BUFFER, there is no problem.
 *
 * State of queues when this kernel is called :
 * At entry,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE and RAY_REGENERATED rays
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with RAY_TO_REGENERATE rays.
 * QUEUE_SHADOW_RAY_CAST_AO_RAYS will be empty.
 * At exit,
 * QUEUE_ACTIVE_AND_REGENERATED_RAYS will be filled with RAY_ACTIVE, RAY_REGENERATED and RAY_UPDATE_BUFFER rays
 * QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS will be filled with RAY_TO_REGENERATE and RAY_UPDATE_BUFFER rays
 * QUEUE_SHADOW_RAY_CAST_AO_RAYS will be filled with rays marked with flag RAY_SHADOW_RAY_CAST_AO
 */
ccl_device void kernel_holdout_emission_blurring_pathtermination_ao(KernelGlobals *kg)
{
	ccl_local unsigned int local_queue_atomics_bg;
	ccl_local unsigned int local_queue_atomics_ao;
	if(ccl_local_id(0) == 0 && ccl_local_id(1) == 0) {
		local_queue_atomics_bg = 0;
		local_queue_atomics_ao = 0;
	}
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

	char enqueue_flag = 0;
	char enqueue_flag_AO_SHADOW_RAY_CAST = 0;
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
	int my_sample_tile;
	unsigned int sample;

	ccl_global RNG *rng = 0x0;
	ccl_global PathState *state = 0x0;
	float3 throughput;

	ccl_global char *ray_state = kernel_split_state.ray_state;
	ShaderData *sd = &kernel_split_state.sd[ray_index];
	ccl_global float *per_sample_output_buffers = kernel_split_state.per_sample_output_buffers;

	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {

		throughput = kernel_split_state.throughput[ray_index];
		state = &kernel_split_state.path_state[ray_index];
		rng = &kernel_split_state.rng[ray_index];

		work_index = kernel_split_state.work_array[ray_index];
		sample = get_work_sample(kg, work_index, ray_index) + kernel_split_params.start_sample;
		get_work_pixel_tile_position(kg, &pixel_x, &pixel_y,
		                        &tile_x, &tile_y,
		                        work_index,
		                        ray_index);
		my_sample_tile = 0;

		per_sample_output_buffers +=
		    ((tile_x + (tile_y * stride)) + my_sample_tile) *
		    kernel_data.film.pass_stride;

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
				ASSIGN_RAY_STATE(ray_state, ray_index, RAY_UPDATE_BUFFER);
				enqueue_flag = 1;
			}
		}
#endif  /* __HOLDOUT__ */
	}

	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
		PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
		/* Holdout mask objects do not write data passes. */
		kernel_write_data_passes(kg,
		                         per_sample_output_buffers,
		                         L,
		                         sd,
		                         sample,
		                         state,
		                         throughput);
		/* Blurring of bsdf after bounces, for rays that have a small likelihood
		 * of following this particular path (diffuse, rough glossy.
		 */
		if(kernel_data.integrator.filter_glossy != FLT_MAX) {
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
		float probability = path_state_terminate_probability(kg, state, throughput);

		if(probability == 0.0f) {
			ASSIGN_RAY_STATE(ray_state, ray_index, RAY_UPDATE_BUFFER);
			enqueue_flag = 1;
		}

		if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
			if(probability != 1.0f) {
				float terminate = path_state_rng_1D_for_decision(kg, rng, state, PRNG_TERMINATE);
				if(terminate >= probability) {
					ASSIGN_RAY_STATE(ray_state, ray_index, RAY_UPDATE_BUFFER);
					enqueue_flag = 1;
				}
				else {
					kernel_split_state.throughput[ray_index] = throughput/probability;
				}
			}
		}
	}

#ifdef __AO__
	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
		/* ambient occlusion */
		if(kernel_data.integrator.use_ambient_occlusion ||
		   (sd->flag & SD_AO))
		{
			/* todo: solve correlation */
			float bsdf_u, bsdf_v;
			path_state_rng_2D(kg, rng, state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);

			float ao_factor = kernel_data.background.ao_factor;
			float3 ao_N;
			kernel_split_state.ao_bsdf[ray_index] = shader_bsdf_ao(kg, sd, ao_factor, &ao_N);
			kernel_split_state.ao_alpha[ray_index] = shader_bsdf_alpha(kg, sd);

			float3 ao_D;
			float ao_pdf;
			sample_cos_hemisphere(ao_N, bsdf_u, bsdf_v, &ao_D, &ao_pdf);

			if(dot(sd->Ng, ao_D) > 0.0f && ao_pdf != 0.0f) {
				Ray _ray;
				_ray.P = ray_offset(sd->P, sd->Ng);
				_ray.D = ao_D;
				_ray.t = kernel_data.background.ao_distance;
#ifdef __OBJECT_MOTION__
				_ray.time = sd->time;
#endif
				_ray.dP = sd->dP;
				_ray.dD = differential3_zero();
				kernel_split_state.ao_light_ray[ray_index] = _ray;

				ADD_RAY_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_AO);
				enqueue_flag_AO_SHADOW_RAY_CAST = 1;
			}
		}
	}
#endif  /* __AO__ */

#ifndef __COMPUTE_DEVICE_GPU__
	}
#endif

	/* Enqueue RAY_UPDATE_BUFFER rays. */
	enqueue_ray_index_local(ray_index,
	                        QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	                        enqueue_flag,
	                        kernel_split_params.queue_size,
	                        &local_queue_atomics_bg,
	                        kernel_split_state.queue_data,
	                        kernel_split_params.queue_index);

#ifdef __AO__
	/* Enqueue to-shadow-ray-cast rays. */
	enqueue_ray_index_local(ray_index,
	                        QUEUE_SHADOW_RAY_CAST_AO_RAYS,
	                        enqueue_flag_AO_SHADOW_RAY_CAST,
	                        kernel_split_params.queue_size,
	                        &local_queue_atomics_ao,
	                        kernel_split_state.queue_data,
	                        kernel_split_params.queue_index);
#endif
}

CCL_NAMESPACE_END

