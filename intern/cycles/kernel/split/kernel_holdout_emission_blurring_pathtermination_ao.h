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
ccl_device void kernel_holdout_emission_blurring_pathtermination_ao(
        KernelGlobals *kg,
        ShaderData *sd,                        /* Required throughout the kernel except probabilistic path termination and AO */
        ccl_global float *per_sample_output_buffers,
        ccl_global uint *rng_coop,             /* Required for "kernel_write_data_passes" and AO */
        ccl_global float3 *throughput_coop,    /* Required for handling holdout material and AO */
        ccl_global float *L_transparent_coop,  /* Required for handling holdout material */
        PathRadiance *PathRadiance_coop,       /* Required for "kernel_write_data_passes" and indirect primitive emission */
        ccl_global PathState *PathState_coop,  /* Required throughout the kernel and AO */
        Intersection *Intersection_coop,       /* Required for indirect primitive emission */
        ccl_global float3 *AOAlpha_coop,       /* Required for AO */
        ccl_global float3 *AOBSDF_coop,        /* Required for AO */
        ccl_global Ray *AOLightRay_coop,       /* Required for AO */
        int sw, int sh, int sx, int sy, int stride,
        ccl_global char *ray_state,            /* Denotes the state of each ray */
        ccl_global unsigned int *work_array,   /* Denotes the work that each ray belongs to */
#ifdef __WORK_STEALING__
        unsigned int start_sample,
#endif
        int parallel_samples,                  /* Number of samples to be processed in parallel */
        int ray_index,
        char *enqueue_flag,
        char *enqueue_flag_AO_SHADOW_RAY_CAST)
{
#ifdef __WORK_STEALING__
	unsigned int my_work;
	unsigned int pixel_x;
	unsigned int pixel_y;
#endif
	unsigned int tile_x;
	unsigned int tile_y;
	int my_sample_tile;
	unsigned int sample;

	ccl_global RNG *rng = 0x0;
	ccl_global PathState *state = 0x0;
	float3 throughput;

	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {

		throughput = throughput_coop[ray_index];
		state = &PathState_coop[ray_index];
		rng = &rng_coop[ray_index];
#ifdef __WORK_STEALING__
		my_work = work_array[ray_index];
		sample = get_my_sample(my_work, sw, sh, parallel_samples, ray_index) + start_sample;
		get_pixel_tile_position(&pixel_x, &pixel_y,
		                        &tile_x, &tile_y,
		                        my_work,
		                        sw, sh, sx, sy,
		                        parallel_samples,
		                        ray_index);
		my_sample_tile = 0;
#else  /* __WORK_STEALING__ */
		sample = work_array[ray_index];
		/* Buffer's stride is "stride"; Find x and y using ray_index. */
		int tile_index = ray_index / parallel_samples;
		tile_x = tile_index % sw;
		tile_y = tile_index / sw;
		my_sample_tile = ray_index - (tile_index * parallel_samples);
#endif  /* __WORK_STEALING__ */
		per_sample_output_buffers +=
		    (((tile_x + (tile_y * stride)) * parallel_samples) + my_sample_tile) *
		    kernel_data.film.pass_stride;

		/* holdout */
#ifdef __HOLDOUT__
		if((ccl_fetch(sd, flag) & (SD_HOLDOUT|SD_HOLDOUT_MASK)) &&
		   (state->flag & PATH_RAY_CAMERA))
		{
			if(kernel_data.background.transparent) {
				float3 holdout_weight;

				if(ccl_fetch(sd, flag) & SD_HOLDOUT_MASK)
					holdout_weight = make_float3(1.0f, 1.0f, 1.0f);
				else
					holdout_weight = shader_holdout_eval(kg, sd);

				/* any throughput is ok, should all be identical here */
				L_transparent_coop[ray_index] += average(holdout_weight*throughput);
			}

			if(ccl_fetch(sd, flag) & SD_HOLDOUT_MASK) {
				ASSIGN_RAY_STATE(ray_state, ray_index, RAY_UPDATE_BUFFER);
				*enqueue_flag = 1;
			}
		}
#endif  /* __HOLDOUT__ */
	}

	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
		PathRadiance *L = &PathRadiance_coop[ray_index];
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
		if(ccl_fetch(sd, flag) & SD_EMISSION) {
			/* TODO(sergey): is isect.t wrong here for transparent surfaces? */
			float3 emission = indirect_primitive_emission(
			        kg,
			        sd,
			        Intersection_coop[ray_index].t,
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
			*enqueue_flag = 1;
		}

		if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
			if(probability != 1.0f) {
				float terminate = path_state_rng_1D_for_decision(kg, rng, state, PRNG_TERMINATE);
				if(terminate >= probability) {
					ASSIGN_RAY_STATE(ray_state, ray_index, RAY_UPDATE_BUFFER);
					*enqueue_flag = 1;
				}
				else {
					throughput_coop[ray_index] = throughput/probability;
				}
			}
		}
	}

#ifdef __AO__
	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
		/* ambient occlusion */
		if(kernel_data.integrator.use_ambient_occlusion ||
		   (ccl_fetch(sd, flag) & SD_AO))
		{
			/* todo: solve correlation */
			float bsdf_u, bsdf_v;
			path_state_rng_2D(kg, rng, state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);

			float ao_factor = kernel_data.background.ao_factor;
			float3 ao_N;
			AOBSDF_coop[ray_index] = shader_bsdf_ao(kg, sd, ao_factor, &ao_N);
			AOAlpha_coop[ray_index] = shader_bsdf_alpha(kg, sd);

			float3 ao_D;
			float ao_pdf;
			sample_cos_hemisphere(ao_N, bsdf_u, bsdf_v, &ao_D, &ao_pdf);

			if(dot(ccl_fetch(sd, Ng), ao_D) > 0.0f && ao_pdf != 0.0f) {
				Ray _ray;
				_ray.P = ray_offset(ccl_fetch(sd, P), ccl_fetch(sd, Ng));
				_ray.D = ao_D;
				_ray.t = kernel_data.background.ao_distance;
#ifdef __OBJECT_MOTION__
				_ray.time = ccl_fetch(sd, time);
#endif
				_ray.dP = ccl_fetch(sd, dP);
				_ray.dD = differential3_zero();
				AOLightRay_coop[ray_index] = _ray;

				ADD_RAY_FLAG(ray_state, ray_index, RAY_SHADOW_RAY_CAST_AO);
				*enqueue_flag_AO_SHADOW_RAY_CAST = 1;
			}
		}
	}
#endif  /* __AO__ */
}
