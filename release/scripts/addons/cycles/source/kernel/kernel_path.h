/*
 * Copyright 2011-2013 Blender Foundation
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

#ifdef __OSL__
#  include "kernel/osl/osl_shader.h"
#endif

#include "kernel/kernel_random.h"
#include "kernel/kernel_projection.h"
#include "kernel/kernel_montecarlo.h"
#include "kernel/kernel_differential.h"
#include "kernel/kernel_camera.h"

#include "kernel/geom/geom.h"
#include "kernel/bvh/bvh.h"

#include "kernel/kernel_accumulate.h"
#include "kernel/kernel_shader.h"
#include "kernel/kernel_light.h"
#include "kernel/kernel_passes.h"

#ifdef __SUBSURFACE__
#  include "kernel/kernel_subsurface.h"
#endif

#ifdef __VOLUME__
#  include "kernel/kernel_volume.h"
#endif

#include "kernel/kernel_path_state.h"
#include "kernel/kernel_shadow.h"
#include "kernel/kernel_emission.h"
#include "kernel/kernel_path_common.h"
#include "kernel/kernel_path_surface.h"
#include "kernel/kernel_path_volume.h"
#include "kernel/kernel_path_subsurface.h"

#ifdef __KERNEL_DEBUG__
#  include "kernel/kernel_debug.h"
#endif

CCL_NAMESPACE_BEGIN

ccl_device_noinline void kernel_path_ao(KernelGlobals *kg,
                                        ShaderData *sd,
                                        ShaderData *emission_sd,
                                        PathRadiance *L,
                                        ccl_addr_space PathState *state,
                                        RNG *rng,
                                        float3 throughput,
                                        float3 ao_alpha)
{
	/* todo: solve correlation */
	float bsdf_u, bsdf_v;

	path_state_rng_2D(kg, rng, state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);

	float ao_factor = kernel_data.background.ao_factor;
	float3 ao_N;
	float3 ao_bsdf = shader_bsdf_ao(kg, sd, ao_factor, &ao_N);
	float3 ao_D;
	float ao_pdf;

	sample_cos_hemisphere(ao_N, bsdf_u, bsdf_v, &ao_D, &ao_pdf);

	if(dot(sd->Ng, ao_D) > 0.0f && ao_pdf != 0.0f) {
		Ray light_ray;
		float3 ao_shadow;

		light_ray.P = ray_offset(sd->P, sd->Ng);
		light_ray.D = ao_D;
		light_ray.t = kernel_data.background.ao_distance;
#ifdef __OBJECT_MOTION__
		light_ray.time = sd->time;
#endif  /* __OBJECT_MOTION__ */
		light_ray.dP = sd->dP;
		light_ray.dD = differential3_zero();

		if(!shadow_blocked(kg, emission_sd, state, &light_ray, &ao_shadow)) {
			path_radiance_accum_ao(L, state, throughput, ao_alpha, ao_bsdf, ao_shadow);
		}
		else {
			path_radiance_accum_total_ao(L, state, throughput, ao_bsdf);
		}
	}
}

#ifndef __SPLIT_KERNEL__

ccl_device void kernel_path_indirect(KernelGlobals *kg,
                                     ShaderData *sd,
                                     ShaderData *emission_sd,
                                     RNG *rng,
                                     Ray *ray,
                                     float3 throughput,
                                     int num_samples,
                                     PathState *state,
                                     PathRadiance *L)
{
	/* path iteration */
	for(;;) {
		/* intersect scene */
		Intersection isect;
		uint visibility = path_state_ray_visibility(kg, state);
		if(state->bounce > kernel_data.integrator.ao_bounces) {
			visibility = PATH_RAY_SHADOW;
			ray->t = kernel_data.background.ao_distance;
		}
		bool hit = scene_intersect(kg,
		                           *ray,
		                           visibility,
		                           &isect,
		                           NULL,
		                           0.0f, 0.0f);

#ifdef __LAMP_MIS__
		if(kernel_data.integrator.use_lamp_mis && !(state->flag & PATH_RAY_CAMERA)) {
			/* ray starting from previous non-transparent bounce */
			Ray light_ray;

			light_ray.P = ray->P - state->ray_t*ray->D;
			state->ray_t += isect.t;
			light_ray.D = ray->D;
			light_ray.t = state->ray_t;
			light_ray.time = ray->time;
			light_ray.dD = ray->dD;
			light_ray.dP = ray->dP;

			/* intersect with lamp */
			float3 emission;
			if(indirect_lamp_emission(kg, emission_sd, state, &light_ray, &emission)) {
				path_radiance_accum_emission(L,
				                             throughput,
				                             emission,
				                             state->bounce);
			}
		}
#endif  /* __LAMP_MIS__ */

#ifdef __VOLUME__
		/* Sanitize volume stack. */
		if(!hit) {
			kernel_volume_clean_stack(kg, state->volume_stack);
		}
		/* volume attenuation, emission, scatter */
		if(state->volume_stack[0].shader != SHADER_NONE) {
			Ray volume_ray = *ray;
			volume_ray.t = (hit)? isect.t: FLT_MAX;

			bool heterogeneous =
			        volume_stack_is_heterogeneous(kg,
			                                      state->volume_stack);

#  ifdef __VOLUME_DECOUPLED__
			int sampling_method =
			        volume_stack_sampling_method(kg,
			                                     state->volume_stack);
			bool decoupled = kernel_volume_use_decoupled(kg, heterogeneous, false, sampling_method);

			if(decoupled) {
				/* cache steps along volume for repeated sampling */
				VolumeSegment volume_segment;

				shader_setup_from_volume(kg,
				                         sd,
				                         &volume_ray);
				kernel_volume_decoupled_record(kg,
				                               state,
				                               &volume_ray,
				                               sd,
				                               &volume_segment,
				                               heterogeneous);

				volume_segment.sampling_method = sampling_method;

				/* emission */
				if(volume_segment.closure_flag & SD_EMISSION) {
					path_radiance_accum_emission(L,
					                             throughput,
					                             volume_segment.accum_emission,
					                             state->bounce);
				}

				/* scattering */
				VolumeIntegrateResult result = VOLUME_PATH_ATTENUATED;

				if(volume_segment.closure_flag & SD_SCATTER) {
					int all = kernel_data.integrator.sample_all_lights_indirect;

					/* direct light sampling */
					kernel_branched_path_volume_connect_light(kg,
					                                          rng,
					                                          sd,
					                                          emission_sd,
					                                          throughput,
					                                          state,
					                                          L,
					                                          all,
					                                          &volume_ray,
					                                          &volume_segment);

					/* indirect sample. if we use distance sampling and take just
					 * one sample for direct and indirect light, we could share
					 * this computation, but makes code a bit complex */
					float rphase = path_state_rng_1D_for_decision(kg, rng, state, PRNG_PHASE);
					float rscatter = path_state_rng_1D_for_decision(kg, rng, state, PRNG_SCATTER_DISTANCE);

					result = kernel_volume_decoupled_scatter(kg,
					                                         state,
					                                         &volume_ray,
					                                         sd,
					                                         &throughput,
					                                         rphase,
					                                         rscatter,
					                                         &volume_segment,
					                                         NULL,
					                                         true);
				}

				/* free cached steps */
				kernel_volume_decoupled_free(kg, &volume_segment);

				if(result == VOLUME_PATH_SCATTERED) {
					if(kernel_path_volume_bounce(kg,
					                             rng,
					                             sd,
					                             &throughput,
					                             state,
					                             L,
					                             ray))
					{
						continue;
					}
					else {
						break;
					}
				}
				else {
					throughput *= volume_segment.accum_transmittance;
				}
			}
			else
#  endif  /* __VOLUME_DECOUPLED__ */
			{
				/* integrate along volume segment with distance sampling */
				VolumeIntegrateResult result = kernel_volume_integrate(
					kg, state, sd, &volume_ray, L, &throughput, rng, heterogeneous);

#  ifdef __VOLUME_SCATTER__
				if(result == VOLUME_PATH_SCATTERED) {
					/* direct lighting */
					kernel_path_volume_connect_light(kg,
					                                 rng,
					                                 sd,
					                                 emission_sd,
					                                 throughput,
					                                 state,
					                                 L);

					/* indirect light bounce */
					if(kernel_path_volume_bounce(kg,
					                             rng,
					                             sd,
					                             &throughput,
					                             state,
					                             L,
					                             ray))
					{
						continue;
					}
					else {
						break;
					}
				}
#  endif  /* __VOLUME_SCATTER__ */
			}
		}
#endif  /* __VOLUME__ */

		if(!hit) {
#ifdef __BACKGROUND__
			/* sample background shader */
			float3 L_background = indirect_background(kg, emission_sd, state, ray);
			path_radiance_accum_background(L,
			                               state,
			                               throughput,
			                               L_background);
#endif  /* __BACKGROUND__ */

			break;
		}
		else if(state->bounce > kernel_data.integrator.ao_bounces) {
			break;
		}

		/* setup shading */
		shader_setup_from_ray(kg,
		                      sd,
		                      &isect,
		                      ray);
		float rbsdf = path_state_rng_1D_for_decision(kg, rng, state, PRNG_BSDF);
		shader_eval_surface(kg, sd, rng, state, rbsdf, state->flag, SHADER_CONTEXT_INDIRECT);
#ifdef __BRANCHED_PATH__
		shader_merge_closures(sd);
#endif  /* __BRANCHED_PATH__ */

#ifdef __SHADOW_TRICKS__
		if(!(sd->object_flag & SD_OBJECT_SHADOW_CATCHER)) {
			state->flag &= ~PATH_RAY_SHADOW_CATCHER_ONLY;
		}
#endif  /* __SHADOW_TRICKS__ */

		/* blurring of bsdf after bounces, for rays that have a small likelihood
		 * of following this particular path (diffuse, rough glossy) */
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
			float3 emission = indirect_primitive_emission(kg,
			                                              sd,
			                                              isect.t,
			                                              state->flag,
			                                              state->ray_pdf);
			path_radiance_accum_emission(L, throughput, emission, state->bounce);
		}
#endif  /* __EMISSION__ */

		/* path termination. this is a strange place to put the termination, it's
		 * mainly due to the mixed in MIS that we use. gives too many unneeded
		 * shader evaluations, only need emission if we are going to terminate */
		float probability =
		        path_state_terminate_probability(kg,
		                                         state,
		                                         throughput*num_samples);

		if(probability == 0.0f) {
			break;
		}
		else if(probability != 1.0f) {
			float terminate = path_state_rng_1D_for_decision(kg, rng, state, PRNG_TERMINATE);

			if(terminate >= probability)
				break;

			throughput /= probability;
		}

		kernel_update_denoising_features(kg, sd, state, L);

#ifdef __AO__
		/* ambient occlusion */
		if(kernel_data.integrator.use_ambient_occlusion || (sd->flag & SD_AO)) {
			kernel_path_ao(kg, sd, emission_sd, L, state, rng, throughput, make_float3(0.0f, 0.0f, 0.0f));
		}
#endif  /* __AO__ */

#ifdef __SUBSURFACE__
		/* bssrdf scatter to a different location on the same object, replacing
		 * the closures with a diffuse BSDF */
		if(sd->flag & SD_BSSRDF) {
			float bssrdf_probability;
			ShaderClosure *sc = subsurface_scatter_pick_closure(kg, sd, &bssrdf_probability);

			/* modify throughput for picking bssrdf or bsdf */
			throughput *= bssrdf_probability;

			/* do bssrdf scatter step if we picked a bssrdf closure */
			if(sc) {
				uint lcg_state = lcg_state_init(rng, state->rng_offset, state->sample, 0x68bc21eb);

				float bssrdf_u, bssrdf_v;
				path_state_rng_2D(kg,
				                  rng,
				                  state,
				                  PRNG_BSDF_U,
				                  &bssrdf_u, &bssrdf_v);
				subsurface_scatter_step(kg,
				                        sd,
				                        state,
				                        state->flag,
				                        sc,
				                        &lcg_state,
				                        bssrdf_u, bssrdf_v,
				                        false);
			}
		}
#endif  /* __SUBSURFACE__ */

#if defined(__EMISSION__)
		if(kernel_data.integrator.use_direct_light) {
			int all = (kernel_data.integrator.sample_all_lights_indirect) ||
			          (state->flag & PATH_RAY_SHADOW_CATCHER);
			kernel_branched_path_surface_connect_light(kg,
			                                           rng,
			                                           sd,
			                                           emission_sd,
			                                           state,
			                                           throughput,
			                                           1.0f,
			                                           L,
			                                           all);
		}
#endif  /* defined(__EMISSION__) */

		if(!kernel_path_surface_bounce(kg, rng, sd, &throughput, state, L, ray))
			break;
	}
}


ccl_device_inline float kernel_path_integrate(KernelGlobals *kg,
                                              RNG *rng,
                                              int sample,
                                              Ray ray,
                                              ccl_global float *buffer,
                                              PathRadiance *L,
                                              bool *is_shadow_catcher)
{
	/* initialize */
	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
	float L_transparent = 0.0f;

	path_radiance_init(L, kernel_data.film.use_light_pass);

	/* shader data memory used for both volumes and surfaces, saves stack space */
	ShaderData sd;
	/* shader data used by emission, shadows, volume stacks */
	ShaderData emission_sd;

	PathState state;
	path_state_init(kg, &emission_sd, &state, rng, sample, &ray);

#ifdef __KERNEL_DEBUG__
	DebugData debug_data;
	debug_data_init(&debug_data);
#endif  /* __KERNEL_DEBUG__ */

#ifdef __SUBSURFACE__
	SubsurfaceIndirectRays ss_indirect;
	kernel_path_subsurface_init_indirect(&ss_indirect);

	for(;;) {
#endif  /* __SUBSURFACE__ */

	/* path iteration */
	for(;;) {
		/* intersect scene */
		Intersection isect;
		uint visibility = path_state_ray_visibility(kg, &state);

#ifdef __HAIR__
		float difl = 0.0f, extmax = 0.0f;
		uint lcg_state = 0;

		if(kernel_data.bvh.have_curves) {
			if((kernel_data.cam.resolution == 1) && (state.flag & PATH_RAY_CAMERA)) {	
				float3 pixdiff = ray.dD.dx + ray.dD.dy;
				/*pixdiff = pixdiff - dot(pixdiff, ray.D)*ray.D;*/
				difl = kernel_data.curve.minimum_width * len(pixdiff) * 0.5f;
			}

			extmax = kernel_data.curve.maximum_width;
			lcg_state = lcg_state_init(rng, state.rng_offset, state.sample, 0x51633e2d);
		}

		if(state.bounce > kernel_data.integrator.ao_bounces) {
			visibility = PATH_RAY_SHADOW;
			ray.t = kernel_data.background.ao_distance;
		}

		bool hit = scene_intersect(kg, ray, visibility, &isect, &lcg_state, difl, extmax);
#else
		bool hit = scene_intersect(kg, ray, visibility, &isect, NULL, 0.0f, 0.0f);
#endif  /* __HAIR__ */

#ifdef __KERNEL_DEBUG__
		if(state.flag & PATH_RAY_CAMERA) {
			debug_data.num_bvh_traversed_nodes += isect.num_traversed_nodes;
			debug_data.num_bvh_traversed_instances += isect.num_traversed_instances;
			debug_data.num_bvh_intersections += isect.num_intersections;
		}
		debug_data.num_ray_bounces++;
#endif  /* __KERNEL_DEBUG__ */

#ifdef __LAMP_MIS__
		if(kernel_data.integrator.use_lamp_mis && !(state.flag & PATH_RAY_CAMERA)) {
			/* ray starting from previous non-transparent bounce */
			Ray light_ray;

			light_ray.P = ray.P - state.ray_t*ray.D;
			state.ray_t += isect.t;
			light_ray.D = ray.D;
			light_ray.t = state.ray_t;
			light_ray.time = ray.time;
			light_ray.dD = ray.dD;
			light_ray.dP = ray.dP;

			/* intersect with lamp */
			float3 emission;

			if(indirect_lamp_emission(kg, &emission_sd, &state, &light_ray, &emission))
				path_radiance_accum_emission(L, throughput, emission, state.bounce);
		}
#endif  /* __LAMP_MIS__ */

#ifdef __VOLUME__
		/* Sanitize volume stack. */
		if(!hit) {
			kernel_volume_clean_stack(kg, state.volume_stack);
		}
		/* volume attenuation, emission, scatter */
		if(state.volume_stack[0].shader != SHADER_NONE) {
			Ray volume_ray = ray;
			volume_ray.t = (hit)? isect.t: FLT_MAX;

			bool heterogeneous = volume_stack_is_heterogeneous(kg, state.volume_stack);

#  ifdef __VOLUME_DECOUPLED__
			int sampling_method = volume_stack_sampling_method(kg, state.volume_stack);
			bool decoupled = kernel_volume_use_decoupled(kg, heterogeneous, true, sampling_method);

			if(decoupled) {
				/* cache steps along volume for repeated sampling */
				VolumeSegment volume_segment;

				shader_setup_from_volume(kg, &sd, &volume_ray);
				kernel_volume_decoupled_record(kg, &state,
					&volume_ray, &sd, &volume_segment, heterogeneous);

				volume_segment.sampling_method = sampling_method;

				/* emission */
				if(volume_segment.closure_flag & SD_EMISSION)
					path_radiance_accum_emission(L, throughput, volume_segment.accum_emission, state.bounce);

				/* scattering */
				VolumeIntegrateResult result = VOLUME_PATH_ATTENUATED;

				if(volume_segment.closure_flag & SD_SCATTER) {
					int all = false;

					/* direct light sampling */
					kernel_branched_path_volume_connect_light(kg, rng, &sd,
						&emission_sd, throughput, &state, L, all,
						&volume_ray, &volume_segment);

					/* indirect sample. if we use distance sampling and take just
					 * one sample for direct and indirect light, we could share
					 * this computation, but makes code a bit complex */
					float rphase = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_PHASE);
					float rscatter = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_SCATTER_DISTANCE);

					result = kernel_volume_decoupled_scatter(kg,
						&state, &volume_ray, &sd, &throughput,
						rphase, rscatter, &volume_segment, NULL, true);
				}

				/* free cached steps */
				kernel_volume_decoupled_free(kg, &volume_segment);

				if(result == VOLUME_PATH_SCATTERED) {
					if(kernel_path_volume_bounce(kg, rng, &sd, &throughput, &state, L, &ray))
						continue;
					else
						break;
				}
				else {
					throughput *= volume_segment.accum_transmittance;
				}
			}
			else
#  endif  /* __VOLUME_DECOUPLED__ */
			{
				/* integrate along volume segment with distance sampling */
				VolumeIntegrateResult result = kernel_volume_integrate(
					kg, &state, &sd, &volume_ray, L, &throughput, rng, heterogeneous);

#  ifdef __VOLUME_SCATTER__
				if(result == VOLUME_PATH_SCATTERED) {
					/* direct lighting */
					kernel_path_volume_connect_light(kg, rng, &sd, &emission_sd, throughput, &state, L);

					/* indirect light bounce */
					if(kernel_path_volume_bounce(kg, rng, &sd, &throughput, &state, L, &ray))
						continue;
					else
						break;
				}
#  endif  /* __VOLUME_SCATTER__ */
			}
		}
#endif  /* __VOLUME__ */

		if(!hit) {
			/* eval background shader if nothing hit */
			if(kernel_data.background.transparent && (state.flag & PATH_RAY_CAMERA)) {
				L_transparent += average(throughput);

#ifdef __PASSES__
				if(!(kernel_data.film.pass_flag & PASS_BACKGROUND))
#endif  /* __PASSES__ */
					break;
			}

#ifdef __BACKGROUND__
			/* sample background shader */
			float3 L_background = indirect_background(kg, &emission_sd, &state, &ray);
			path_radiance_accum_background(L, &state, throughput, L_background);
#endif  /* __BACKGROUND__ */

			break;
		}
		else if(state.bounce > kernel_data.integrator.ao_bounces) {
			break;
		}

		/* setup shading */
		shader_setup_from_ray(kg, &sd, &isect, &ray);
		float rbsdf = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_BSDF);
		shader_eval_surface(kg, &sd, rng, &state, rbsdf, state.flag, SHADER_CONTEXT_MAIN);

#ifdef __SHADOW_TRICKS__
		if((sd.object_flag & SD_OBJECT_SHADOW_CATCHER)) {
			if(state.flag & PATH_RAY_CAMERA) {
				state.flag |= (PATH_RAY_SHADOW_CATCHER |
				               PATH_RAY_SHADOW_CATCHER_ONLY |
				               PATH_RAY_STORE_SHADOW_INFO);
				state.catcher_object = sd.object;
				if(!kernel_data.background.transparent) {
					L->shadow_background_color =
					        indirect_background(kg, &emission_sd, &state, &ray);
				}
				L->shadow_radiance_sum = path_radiance_clamp_and_sum(kg, L);
				L->shadow_throughput = average(throughput);
			}
		}
		else {
			state.flag &= ~PATH_RAY_SHADOW_CATCHER_ONLY;
		}
#endif  /* __SHADOW_TRICKS__ */

		/* holdout */
#ifdef __HOLDOUT__
		if(((sd.flag & SD_HOLDOUT) ||
		    (sd.object_flag & SD_OBJECT_HOLDOUT_MASK)) &&
		   (state.flag & PATH_RAY_CAMERA))
		{
			if(kernel_data.background.transparent) {
				float3 holdout_weight;
				if(sd.object_flag & SD_OBJECT_HOLDOUT_MASK) {
					holdout_weight = make_float3(1.0f, 1.0f, 1.0f);
				}
				else {
					holdout_weight = shader_holdout_eval(kg, &sd);
				}
				/* any throughput is ok, should all be identical here */
				L_transparent += average(holdout_weight*throughput);
			}

			if(sd.object_flag & SD_OBJECT_HOLDOUT_MASK) {
				break;
			}
		}
#endif  /* __HOLDOUT__ */

		/* holdout mask objects do not write data passes */
		kernel_write_data_passes(kg, buffer, L, &sd, sample, &state, throughput);

		/* blurring of bsdf after bounces, for rays that have a small likelihood
		 * of following this particular path (diffuse, rough glossy) */
		if(kernel_data.integrator.filter_glossy != FLT_MAX) {
			float blur_pdf = kernel_data.integrator.filter_glossy*state.min_ray_pdf;

			if(blur_pdf < 1.0f) {
				float blur_roughness = sqrtf(1.0f - blur_pdf)*0.5f;
				shader_bsdf_blur(kg, &sd, blur_roughness);
			}
		}

#ifdef __EMISSION__
		/* emission */
		if(sd.flag & SD_EMISSION) {
			/* todo: is isect.t wrong here for transparent surfaces? */
			float3 emission = indirect_primitive_emission(kg, &sd, isect.t, state.flag, state.ray_pdf);
			path_radiance_accum_emission(L, throughput, emission, state.bounce);
		}
#endif  /* __EMISSION__ */

		/* path termination. this is a strange place to put the termination, it's
		 * mainly due to the mixed in MIS that we use. gives too many unneeded
		 * shader evaluations, only need emission if we are going to terminate */
		float probability = path_state_terminate_probability(kg, &state, throughput);

		if(probability == 0.0f) {
			break;
		}
		else if(probability != 1.0f) {
			float terminate = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_TERMINATE);
			if(terminate >= probability)
				break;

			throughput /= probability;
		}

		kernel_update_denoising_features(kg, &sd, &state, L);

#ifdef __AO__
		/* ambient occlusion */
		if(kernel_data.integrator.use_ambient_occlusion || (sd.flag & SD_AO)) {
			kernel_path_ao(kg, &sd, &emission_sd, L, &state, rng, throughput, shader_bsdf_alpha(kg, &sd));
		}
#endif  /* __AO__ */

#ifdef __SUBSURFACE__
		/* bssrdf scatter to a different location on the same object, replacing
		 * the closures with a diffuse BSDF */
		if(sd.flag & SD_BSSRDF) {
			if(kernel_path_subsurface_scatter(kg,
			                                  &sd,
			                                  &emission_sd,
			                                  L,
			                                  &state,
			                                  rng,
			                                  &ray,
			                                  &throughput,
			                                  &ss_indirect))
			{
				break;
			}
		}
#endif  /* __SUBSURFACE__ */

		/* direct lighting */
		kernel_path_surface_connect_light(kg, rng, &sd, &emission_sd, throughput, &state, L);

		/* compute direct lighting and next bounce */
		if(!kernel_path_surface_bounce(kg, rng, &sd, &throughput, &state, L, &ray))
			break;
	}

#ifdef __SUBSURFACE__
		kernel_path_subsurface_accum_indirect(&ss_indirect, L);

		/* Trace indirect subsurface rays by restarting the loop. this uses less
		 * stack memory than invoking kernel_path_indirect.
		 */
		if(ss_indirect.num_rays) {
			kernel_path_subsurface_setup_indirect(kg,
			                                      &ss_indirect,
			                                      &state,
			                                      &ray,
			                                      L,
			                                      &throughput);
		}
		else {
			break;
		}
	}
#endif  /* __SUBSURFACE__ */

#ifdef __SHADOW_TRICKS__
	*is_shadow_catcher = (state.flag & PATH_RAY_SHADOW_CATCHER);
#endif  /* __SHADOW_TRICKS__ */

#ifdef __KERNEL_DEBUG__
	kernel_write_debug_passes(kg, buffer, &state, &debug_data, sample);
#endif  /* __KERNEL_DEBUG__ */

	return 1.0f - L_transparent;
}

ccl_device void kernel_path_trace(KernelGlobals *kg,
	ccl_global float *buffer, ccl_global uint *rng_state,
	int sample, int x, int y, int offset, int stride)
{
	/* buffer offset */
	int index = offset + x + y*stride;
	int pass_stride = kernel_data.film.pass_stride;

	rng_state += index;
	buffer += index*pass_stride;

	/* initialize random numbers and ray */
	RNG rng;
	Ray ray;

	kernel_path_trace_setup(kg, rng_state, sample, x, y, &rng, &ray);

	/* integrate */
	PathRadiance L;
	bool is_shadow_catcher;

	if(ray.t != 0.0f) {
		float alpha = kernel_path_integrate(kg, &rng, sample, ray, buffer, &L, &is_shadow_catcher);
		kernel_write_result(kg, buffer, sample, &L, alpha, is_shadow_catcher);
	}
	else {
		kernel_write_result(kg, buffer, sample, NULL, 0.0f, false);
	}

	path_rng_end(kg, rng_state, rng);
}

#endif  /* __SPLIT_KERNEL__ */

CCL_NAMESPACE_END

