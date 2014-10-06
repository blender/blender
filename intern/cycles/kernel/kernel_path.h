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
 * limitations under the License
 */

#ifdef __OSL__
#include "osl_shader.h"
#endif

#include "kernel_random.h"
#include "kernel_projection.h"
#include "kernel_montecarlo.h"
#include "kernel_differential.h"
#include "kernel_camera.h"

#include "geom/geom.h"

#include "kernel_accumulate.h"
#include "kernel_shader.h"
#include "kernel_light.h"
#include "kernel_passes.h"

#ifdef __SUBSURFACE__
#include "kernel_subsurface.h"
#endif

#ifdef __VOLUME__
#include "kernel_volume.h"
#endif

#include "kernel_path_state.h"
#include "kernel_shadow.h"
#include "kernel_emission.h"
#include "kernel_path_surface.h"
#include "kernel_path_volume.h"

#ifdef __KERNEL_DEBUG__
#include "kernel_debug.h"
#endif

CCL_NAMESPACE_BEGIN

ccl_device void kernel_path_indirect(KernelGlobals *kg, RNG *rng, Ray ray,
	float3 throughput, int num_samples, PathState state, PathRadiance *L)
{
	/* path iteration */
	for(;;) {
		/* intersect scene */
		Intersection isect;
		uint visibility = path_state_ray_visibility(kg, &state);
		bool hit = scene_intersect(kg, &ray, visibility, &isect, NULL, 0.0f, 0.0f);

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

			if(indirect_lamp_emission(kg, &state, &light_ray, &emission))
				path_radiance_accum_emission(L, throughput, emission, state.bounce);
		}
#endif

#ifdef __VOLUME__
		/* volume attenuation, emission, scatter */
		if(state.volume_stack[0].shader != SHADER_NONE) {
			Ray volume_ray = ray;
			volume_ray.t = (hit)? isect.t: FLT_MAX;

			bool heterogeneous = volume_stack_is_heterogeneous(kg, state.volume_stack);

#ifdef __VOLUME_DECOUPLED__
			int sampling_method = volume_stack_sampling_method(kg, state.volume_stack);
			bool decoupled = kernel_volume_use_decoupled(kg, heterogeneous, false, sampling_method);

			if(decoupled) {
				/* cache steps along volume for repeated sampling */
				VolumeSegment volume_segment;
				ShaderData volume_sd;

				shader_setup_from_volume(kg, &volume_sd, &volume_ray, state.bounce, state.transparent_bounce);
				kernel_volume_decoupled_record(kg, &state,
					&volume_ray, &volume_sd, &volume_segment, heterogeneous);
				
				volume_segment.sampling_method = sampling_method;

				/* emission */
				if(volume_segment.closure_flag & SD_EMISSION)
					path_radiance_accum_emission(L, throughput, volume_segment.accum_emission, state.bounce);

				/* scattering */
				VolumeIntegrateResult result = VOLUME_PATH_ATTENUATED;

				if(volume_segment.closure_flag & SD_SCATTER) {
					bool all = kernel_data.integrator.sample_all_lights_indirect;

					/* direct light sampling */
					kernel_branched_path_volume_connect_light(kg, rng, &volume_sd,
						throughput, &state, L, 1.0f, all, &volume_ray, &volume_segment);

					/* indirect sample. if we use distance sampling and take just
					 * one sample for direct and indirect light, we could share
					 * this computation, but makes code a bit complex */
					float rphase = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_PHASE);
					float rscatter = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_SCATTER_DISTANCE);

					result = kernel_volume_decoupled_scatter(kg,
						&state, &volume_ray, &volume_sd, &throughput,
						rphase, rscatter, &volume_segment, NULL, true);
				}

				if(result != VOLUME_PATH_SCATTERED)
					throughput *= volume_segment.accum_transmittance;

				/* free cached steps */
				kernel_volume_decoupled_free(kg, &volume_segment);

				if(result == VOLUME_PATH_SCATTERED) {
					if(kernel_path_volume_bounce(kg, rng, &volume_sd, &throughput, &state, L, &ray))
						continue;
					else
						break;
				}
			}
			else
#endif
			{
				/* integrate along volume segment with distance sampling */
				ShaderData volume_sd;
				VolumeIntegrateResult result = kernel_volume_integrate(
					kg, &state, &volume_sd, &volume_ray, L, &throughput, rng, heterogeneous);

#ifdef __VOLUME_SCATTER__
				if(result == VOLUME_PATH_SCATTERED) {
					/* direct lighting */
					kernel_path_volume_connect_light(kg, rng, &volume_sd, throughput, &state, L);

					/* indirect light bounce */
					if(kernel_path_volume_bounce(kg, rng, &volume_sd, &throughput, &state, L, &ray))
						continue;
					else
						break;
				}
#endif
			}
		}
#endif

		if(!hit) {
#ifdef __BACKGROUND__
			/* sample background shader */
			float3 L_background = indirect_background(kg, &state, &ray);
			path_radiance_accum_background(L, throughput, L_background, state.bounce);
#endif

			break;
		}

		/* setup shading */
		ShaderData sd;
		shader_setup_from_ray(kg, &sd, &isect, &ray, state.bounce, state.transparent_bounce);
		float rbsdf = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_BSDF);
		shader_eval_surface(kg, &sd, rbsdf, state.flag, SHADER_CONTEXT_INDIRECT);
#ifdef __BRANCHED_PATH__
		shader_merge_closures(&sd);
#endif

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
			float3 emission = indirect_primitive_emission(kg, &sd, isect.t, state.flag, state.ray_pdf);
			path_radiance_accum_emission(L, throughput, emission, state.bounce);
		}
#endif

		/* path termination. this is a strange place to put the termination, it's
		 * mainly due to the mixed in MIS that we use. gives too many unneeded
		 * shader evaluations, only need emission if we are going to terminate */
		float probability = path_state_terminate_probability(kg, &state, throughput*num_samples);

		if(probability == 0.0f) {
			break;
		}
		else if(probability != 1.0f) {
			float terminate = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_TERMINATE);

			if(terminate >= probability)
				break;

			throughput /= probability;
		}

#ifdef __AO__
		/* ambient occlusion */
		if(kernel_data.integrator.use_ambient_occlusion || (sd.flag & SD_AO)) {
			float bsdf_u, bsdf_v;
			path_state_rng_2D(kg, rng, &state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);

			float ao_factor = kernel_data.background.ao_factor;
			float3 ao_N;
			float3 ao_bsdf = shader_bsdf_ao(kg, &sd, ao_factor, &ao_N);
			float3 ao_D;
			float ao_pdf;
			float3 ao_alpha = make_float3(0.0f, 0.0f, 0.0f);

			sample_cos_hemisphere(ao_N, bsdf_u, bsdf_v, &ao_D, &ao_pdf);

			if(dot(sd.Ng, ao_D) > 0.0f && ao_pdf != 0.0f) {
				Ray light_ray;
				float3 ao_shadow;

				light_ray.P = ray_offset(sd.P, sd.Ng);
				light_ray.D = ao_D;
				light_ray.t = kernel_data.background.ao_distance;
#ifdef __OBJECT_MOTION__
				light_ray.time = sd.time;
#endif
				light_ray.dP = sd.dP;
				light_ray.dD = differential3_zero();

				if(!shadow_blocked(kg, &state, &light_ray, &ao_shadow))
					path_radiance_accum_ao(L, throughput, ao_alpha, ao_bsdf, ao_shadow, state.bounce);
			}
		}
#endif

#ifdef __SUBSURFACE__
		/* bssrdf scatter to a different location on the same object, replacing
		 * the closures with a diffuse BSDF */
		if(sd.flag & SD_BSSRDF) {
			float bssrdf_probability;
			ShaderClosure *sc = subsurface_scatter_pick_closure(kg, &sd, &bssrdf_probability);

			/* modify throughput for picking bssrdf or bsdf */
			throughput *= bssrdf_probability;

			/* do bssrdf scatter step if we picked a bssrdf closure */
			if(sc) {
				uint lcg_state = lcg_state_init(rng, &state, 0x68bc21eb);

				float bssrdf_u, bssrdf_v;
				path_state_rng_2D(kg, rng, &state, PRNG_BSDF_U, &bssrdf_u, &bssrdf_v);
				subsurface_scatter_step(kg, &sd, state.flag, sc, &lcg_state, bssrdf_u, bssrdf_v, false);

				state.flag |= PATH_RAY_BSSRDF_ANCESTOR;
			}
		}
#endif

#if defined(__EMISSION__) && defined(__BRANCHED_PATH__)
		if(kernel_data.integrator.use_direct_light) {
			bool all = kernel_data.integrator.sample_all_lights_indirect;
			kernel_branched_path_surface_connect_light(kg, rng, &sd, &state, throughput, 1.0f, L, all);
		}
#endif

		if(!kernel_path_surface_bounce(kg, rng, &sd, &throughput, &state, L, &ray))
			break;
	}
}

ccl_device void kernel_path_ao(KernelGlobals *kg, ShaderData *sd, PathRadiance *L, PathState *state, RNG *rng, float3 throughput)
{
	/* todo: solve correlation */
	float bsdf_u, bsdf_v;

	path_state_rng_2D(kg, rng, state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);

	float ao_factor = kernel_data.background.ao_factor;
	float3 ao_N;
	float3 ao_bsdf = shader_bsdf_ao(kg, sd, ao_factor, &ao_N);
	float3 ao_D;
	float ao_pdf;
	float3 ao_alpha = shader_bsdf_alpha(kg, sd);

	sample_cos_hemisphere(ao_N, bsdf_u, bsdf_v, &ao_D, &ao_pdf);

	if(dot(sd->Ng, ao_D) > 0.0f && ao_pdf != 0.0f) {
		Ray light_ray;
		float3 ao_shadow;

		light_ray.P = ray_offset(sd->P, sd->Ng);
		light_ray.D = ao_D;
		light_ray.t = kernel_data.background.ao_distance;
#ifdef __OBJECT_MOTION__
		light_ray.time = sd->time;
#endif
		light_ray.dP = sd->dP;
		light_ray.dD = differential3_zero();

		if(!shadow_blocked(kg, state, &light_ray, &ao_shadow))
			path_radiance_accum_ao(L, throughput, ao_alpha, ao_bsdf, ao_shadow, state->bounce);
	}
}

ccl_device void kernel_branched_path_ao(KernelGlobals *kg, ShaderData *sd, PathRadiance *L, PathState *state, RNG *rng, float3 throughput)
{
	int num_samples = kernel_data.integrator.ao_samples;
	float num_samples_inv = 1.0f/num_samples;
	float ao_factor = kernel_data.background.ao_factor;
	float3 ao_N;
	float3 ao_bsdf = shader_bsdf_ao(kg, sd, ao_factor, &ao_N);
	float3 ao_alpha = shader_bsdf_alpha(kg, sd);

	for(int j = 0; j < num_samples; j++) {
		float bsdf_u, bsdf_v;
		path_branched_rng_2D(kg, rng, state, j, num_samples, PRNG_BSDF_U, &bsdf_u, &bsdf_v);

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
#endif
			light_ray.dP = sd->dP;
			light_ray.dD = differential3_zero();

			if(!shadow_blocked(kg, state, &light_ray, &ao_shadow))
				path_radiance_accum_ao(L, throughput*num_samples_inv, ao_alpha, ao_bsdf, ao_shadow, state->bounce);
		}
	}
}

#ifdef __SUBSURFACE__

#ifdef __VOLUME__
ccl_device void kernel_path_subsurface_update_volume_stack(KernelGlobals *kg,
                                                           Ray *ray,
                                                           VolumeStack *stack)
{
	kernel_assert(kernel_data.integrator.use_volumes);

	Ray volume_ray = *ray;
	Intersection isect;

	while(scene_intersect_volume(kg, &volume_ray, &isect))
	{
		ShaderData sd;
		shader_setup_from_ray(kg, &sd, &isect, &volume_ray, 0, 0);
		kernel_volume_stack_enter_exit(kg, &sd, stack);

		/* Move ray forward. */
		volume_ray.P = ray_offset(sd.P, -sd.Ng);
		volume_ray.t -= sd.ray_length;
	}
}
#endif

ccl_device bool kernel_path_subsurface_scatter(KernelGlobals *kg, ShaderData *sd, PathRadiance *L, PathState *state, RNG *rng, Ray *ray, float3 *throughput)
{
	float bssrdf_probability;
	ShaderClosure *sc = subsurface_scatter_pick_closure(kg, sd, &bssrdf_probability);

	/* modify throughput for picking bssrdf or bsdf */
	*throughput *= bssrdf_probability;

	/* do bssrdf scatter step if we picked a bssrdf closure */
	if(sc) {
		uint lcg_state = lcg_state_init(rng, state, 0x68bc21eb);

		ShaderData bssrdf_sd[BSSRDF_MAX_HITS];
		float bssrdf_u, bssrdf_v;
		path_state_rng_2D(kg, rng, state, PRNG_BSDF_U, &bssrdf_u, &bssrdf_v);
		int num_hits = subsurface_scatter_multi_step(kg, sd, bssrdf_sd, state->flag, sc, &lcg_state, bssrdf_u, bssrdf_v, false);
#ifdef __VOLUME__
		Ray volume_ray = *ray;
		bool need_update_volume_stack = kernel_data.integrator.use_volumes &&
		                                sd->flag & SD_OBJECT_INTERSECTS_VOLUME;
#endif

		/* compute lighting with the BSDF closure */
		for(int hit = 0; hit < num_hits; hit++) {
			float3 tp = *throughput;
			PathState hit_state = *state;
			Ray hit_ray = *ray;

			hit_state.flag |= PATH_RAY_BSSRDF_ANCESTOR;
			hit_state.rng_offset += PRNG_BOUNCE_NUM;
			
			kernel_path_surface_connect_light(kg, rng, &bssrdf_sd[hit], tp, state, L);

			if(kernel_path_surface_bounce(kg, rng, &bssrdf_sd[hit], &tp, &hit_state, L, &hit_ray)) {
#ifdef __LAMP_MIS__
				hit_state.ray_t = 0.0f;
#endif

#ifdef __VOLUME__
				if(need_update_volume_stack) {
					/* Setup ray from previous surface point to the new one. */
					volume_ray.D = normalize_len(hit_ray.P - volume_ray.P,
					                             &volume_ray.t);

					kernel_path_subsurface_update_volume_stack(
					    kg,
					    &volume_ray,
					    hit_state.volume_stack);

					/* Move volume ray forward. */
					volume_ray.P = hit_ray.P;
				}
#endif

				kernel_path_indirect(kg, rng, hit_ray, tp, state->num_samples, hit_state, L);

				/* for render passes, sum and reset indirect light pass variables
				 * for the next samples */
				path_radiance_sum_indirect(L);
				path_radiance_reset_indirect(L);
			}
		}
		return true;
	}
	return false;
}
#endif

ccl_device float4 kernel_path_integrate(KernelGlobals *kg, RNG *rng, int sample, Ray ray, ccl_global float *buffer)
{
	/* initialize */
	PathRadiance L;
	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
	float L_transparent = 0.0f;

	path_radiance_init(&L, kernel_data.film.use_light_pass);

	PathState state;
	path_state_init(kg, &state, rng, sample, &ray);

#ifdef __KERNEL_DEBUG__
	DebugData debug_data;
	debug_data_init(&debug_data);
#endif

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
			lcg_state = lcg_state_init(rng, &state, 0x51633e2d);
		}

		bool hit = scene_intersect(kg, &ray, visibility, &isect, &lcg_state, difl, extmax);
#else
		bool hit = scene_intersect(kg, &ray, visibility, &isect, NULL, 0.0f, 0.0f);
#endif

#ifdef __KERNEL_DEBUG__
		if(state.flag & PATH_RAY_CAMERA) {
			debug_data.num_bvh_traversal_steps += isect.num_traversal_steps;
		}
#endif

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

			if(indirect_lamp_emission(kg, &state, &light_ray, &emission))
				path_radiance_accum_emission(&L, throughput, emission, state.bounce);
		}
#endif

#ifdef __VOLUME__
		/* volume attenuation, emission, scatter */
		if(state.volume_stack[0].shader != SHADER_NONE) {
			Ray volume_ray = ray;
			volume_ray.t = (hit)? isect.t: FLT_MAX;

			bool heterogeneous = volume_stack_is_heterogeneous(kg, state.volume_stack);

#ifdef __VOLUME_DECOUPLED__
			int sampling_method = volume_stack_sampling_method(kg, state.volume_stack);
			bool decoupled = kernel_volume_use_decoupled(kg, heterogeneous, true, sampling_method);

			if(decoupled) {
				/* cache steps along volume for repeated sampling */
				VolumeSegment volume_segment;
				ShaderData volume_sd;

				shader_setup_from_volume(kg, &volume_sd, &volume_ray, state.bounce, state.transparent_bounce);
				kernel_volume_decoupled_record(kg, &state,
					&volume_ray, &volume_sd, &volume_segment, heterogeneous);

				volume_segment.sampling_method = sampling_method;

				/* emission */
				if(volume_segment.closure_flag & SD_EMISSION)
					path_radiance_accum_emission(&L, throughput, volume_segment.accum_emission, state.bounce);

				/* scattering */
				VolumeIntegrateResult result = VOLUME_PATH_ATTENUATED;

				if(volume_segment.closure_flag & SD_SCATTER) {
					bool all = false;

					/* direct light sampling */
					kernel_branched_path_volume_connect_light(kg, rng, &volume_sd,
						throughput, &state, &L, 1.0f, all, &volume_ray, &volume_segment);

					/* indirect sample. if we use distance sampling and take just
					 * one sample for direct and indirect light, we could share
					 * this computation, but makes code a bit complex */
					float rphase = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_PHASE);
					float rscatter = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_SCATTER_DISTANCE);

					result = kernel_volume_decoupled_scatter(kg,
						&state, &volume_ray, &volume_sd, &throughput,
						rphase, rscatter, &volume_segment, NULL, true);
				}

				if(result != VOLUME_PATH_SCATTERED)
					throughput *= volume_segment.accum_transmittance;

				/* free cached steps */
				kernel_volume_decoupled_free(kg, &volume_segment);

				if(result == VOLUME_PATH_SCATTERED) {
					if(kernel_path_volume_bounce(kg, rng, &volume_sd, &throughput, &state, &L, &ray))
						continue;
					else
						break;
				}
			}
			else 
#endif
			{
				/* integrate along volume segment with distance sampling */
				ShaderData volume_sd;
				VolumeIntegrateResult result = kernel_volume_integrate(
					kg, &state, &volume_sd, &volume_ray, &L, &throughput, rng, heterogeneous);

#ifdef __VOLUME_SCATTER__
				if(result == VOLUME_PATH_SCATTERED) {
					/* direct lighting */
					kernel_path_volume_connect_light(kg, rng, &volume_sd, throughput, &state, &L);

					/* indirect light bounce */
					if(kernel_path_volume_bounce(kg, rng, &volume_sd, &throughput, &state, &L, &ray))
						continue;
					else
						break;
				}
#endif
			}
		}
#endif

		if(!hit) {
			/* eval background shader if nothing hit */
			if(kernel_data.background.transparent && (state.flag & PATH_RAY_CAMERA)) {
				L_transparent += average(throughput);

#ifdef __PASSES__
				if(!(kernel_data.film.pass_flag & PASS_BACKGROUND))
#endif
					break;
			}

#ifdef __BACKGROUND__
			/* sample background shader */
			float3 L_background = indirect_background(kg, &state, &ray);
			path_radiance_accum_background(&L, throughput, L_background, state.bounce);
#endif

			break;
		}

		/* setup shading */
		ShaderData sd;
		shader_setup_from_ray(kg, &sd, &isect, &ray, state.bounce, state.transparent_bounce);
		float rbsdf = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_BSDF);
		shader_eval_surface(kg, &sd, rbsdf, state.flag, SHADER_CONTEXT_MAIN);

		/* holdout */
#ifdef __HOLDOUT__
		if((sd.flag & (SD_HOLDOUT|SD_HOLDOUT_MASK)) && (state.flag & PATH_RAY_CAMERA)) {
			if(kernel_data.background.transparent) {
				float3 holdout_weight;
				
				if(sd.flag & SD_HOLDOUT_MASK)
					holdout_weight = make_float3(1.0f, 1.0f, 1.0f);
				else
					holdout_weight = shader_holdout_eval(kg, &sd);

				/* any throughput is ok, should all be identical here */
				L_transparent += average(holdout_weight*throughput);
			}

			if(sd.flag & SD_HOLDOUT_MASK)
				break;
		}
#endif

		/* holdout mask objects do not write data passes */
		kernel_write_data_passes(kg, buffer, &L, &sd, sample, &state, throughput);

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
			path_radiance_accum_emission(&L, throughput, emission, state.bounce);
		}
#endif

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

#ifdef __AO__
		/* ambient occlusion */
		if(kernel_data.integrator.use_ambient_occlusion || (sd.flag & SD_AO)) {
			kernel_path_ao(kg, &sd, &L, &state, rng, throughput);
		}
#endif

#ifdef __SUBSURFACE__
		/* bssrdf scatter to a different location on the same object, replacing
		 * the closures with a diffuse BSDF */
		if(sd.flag & SD_BSSRDF) {
			if(kernel_path_subsurface_scatter(kg, &sd, &L, &state, rng, &ray, &throughput))
				break;
		}
#endif

		/* direct lighting */
		kernel_path_surface_connect_light(kg, rng, &sd, throughput, &state, &L);

		/* compute direct lighting and next bounce */
		if(!kernel_path_surface_bounce(kg, rng, &sd, &throughput, &state, &L, &ray))
			break;
	}

	float3 L_sum = path_radiance_clamp_and_sum(kg, &L);

	kernel_write_light_passes(kg, buffer, &L, sample);

#ifdef __KERNEL_DEBUG__
	kernel_write_debug_passes(kg, buffer, &state, &debug_data, sample);
#endif

	return make_float4(L_sum.x, L_sum.y, L_sum.z, 1.0f - L_transparent);
}

#ifdef __BRANCHED_PATH__

/* branched path tracing: bounce off surface and integrate indirect light */
ccl_device_noinline void kernel_branched_path_surface_indirect_light(KernelGlobals *kg,
	RNG *rng, ShaderData *sd, float3 throughput, float num_samples_adjust,
	PathState *state, PathRadiance *L)
{
	for(int i = 0; i< sd->num_closure; i++) {
		const ShaderClosure *sc = &sd->closure[i];

		if(!CLOSURE_IS_BSDF(sc->type))
			continue;
		/* transparency is not handled here, but in outer loop */
		if(sc->type == CLOSURE_BSDF_TRANSPARENT_ID)
			continue;

		int num_samples;

		if(CLOSURE_IS_BSDF_DIFFUSE(sc->type))
			num_samples = kernel_data.integrator.diffuse_samples;
		else if(CLOSURE_IS_BSDF_BSSRDF(sc->type))
			num_samples = 1;
		else if(CLOSURE_IS_BSDF_GLOSSY(sc->type))
			num_samples = kernel_data.integrator.glossy_samples;
		else
			num_samples = kernel_data.integrator.transmission_samples;

		num_samples = ceil_to_int(num_samples_adjust*num_samples);

		float num_samples_inv = num_samples_adjust/num_samples;
		RNG bsdf_rng = cmj_hash(*rng, i);

		for(int j = 0; j < num_samples; j++) {
			PathState ps = *state;
			float3 tp = throughput;
			Ray bsdf_ray;

			if(!kernel_branched_path_surface_bounce(kg, &bsdf_rng, sd, sc, j, num_samples, &tp, &ps, L, &bsdf_ray))
				continue;

			kernel_path_indirect(kg, rng, bsdf_ray, tp*num_samples_inv, num_samples, ps, L);

			/* for render passes, sum and reset indirect light pass variables
			 * for the next samples */
			path_radiance_sum_indirect(L);
			path_radiance_reset_indirect(L);
		}
	}
}

#ifdef __SUBSURFACE__
ccl_device void kernel_branched_path_subsurface_scatter(KernelGlobals *kg,
                                                        ShaderData *sd,
                                                        PathRadiance *L,
                                                        PathState *state,
                                                        RNG *rng,
                                                        Ray *ray,
                                                        float3 throughput)
{
	for(int i = 0; i< sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(!CLOSURE_IS_BSSRDF(sc->type))
			continue;

		/* set up random number generator */
		uint lcg_state = lcg_state_init(rng, state, 0x68bc21eb);
		int num_samples = kernel_data.integrator.subsurface_samples;
		float num_samples_inv = 1.0f/num_samples;
		RNG bssrdf_rng = cmj_hash(*rng, i);

		state->flag |= PATH_RAY_BSSRDF_ANCESTOR;

		/* do subsurface scatter step with copy of shader data, this will
		 * replace the BSSRDF with a diffuse BSDF closure */
		for(int j = 0; j < num_samples; j++) {
			ShaderData bssrdf_sd[BSSRDF_MAX_HITS];
			float bssrdf_u, bssrdf_v;
			path_branched_rng_2D(kg, &bssrdf_rng, state, j, num_samples, PRNG_BSDF_U, &bssrdf_u, &bssrdf_v);
			int num_hits = subsurface_scatter_multi_step(kg, sd, bssrdf_sd, state->flag, sc, &lcg_state, bssrdf_u, bssrdf_v, true);
#ifdef __VOLUME__
			Ray volume_ray = *ray;
			bool need_update_volume_stack = kernel_data.integrator.use_volumes &&
			                                sd->flag & SD_OBJECT_INTERSECTS_VOLUME;
#endif

			/* compute lighting with the BSDF closure */
			for(int hit = 0; hit < num_hits; hit++) {
				PathState hit_state = *state;

				path_state_branch(&hit_state, j, num_samples);

#ifdef __VOLUME__
				if(need_update_volume_stack) {
					/* Setup ray from previous surface point to the new one. */
					float3 P = ray_offset(bssrdf_sd[hit].P, -bssrdf_sd[hit].Ng);
					volume_ray.D = normalize_len(P - volume_ray.P,
					                             &volume_ray.t);

					kernel_path_subsurface_update_volume_stack(
					    kg,
					    &volume_ray,
					    hit_state.volume_stack);

					/* Move volume ray forward. */
					volume_ray.P = P;
				}
#endif

#if defined(__EMISSION__) && defined(__BRANCHED_PATH__)
				/* direct light */
				if(kernel_data.integrator.use_direct_light) {
					bool all = kernel_data.integrator.sample_all_lights_direct;
					kernel_branched_path_surface_connect_light(kg, rng,
						&bssrdf_sd[hit], &hit_state, throughput, num_samples_inv, L, all);
				}
#endif

				/* indirect light */
				kernel_branched_path_surface_indirect_light(kg, rng,
					&bssrdf_sd[hit], throughput, num_samples_inv,
					&hit_state, L);
			}
		}

		state->flag &= ~PATH_RAY_BSSRDF_ANCESTOR;
	}
}
#endif

ccl_device float4 kernel_branched_path_integrate(KernelGlobals *kg, RNG *rng, int sample, Ray ray, ccl_global float *buffer)
{
	/* initialize */
	PathRadiance L;
	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
	float L_transparent = 0.0f;

	path_radiance_init(&L, kernel_data.film.use_light_pass);

	PathState state;
	path_state_init(kg, &state, rng, sample, &ray);

#ifdef __KERNEL_DEBUG__
	DebugData debug_data;
	debug_data_init(&debug_data);
#endif

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
			lcg_state = lcg_state_init(rng, &state, 0x51633e2d);
		}

		bool hit = scene_intersect(kg, &ray, visibility, &isect, &lcg_state, difl, extmax);
#else
		bool hit = scene_intersect(kg, &ray, visibility, &isect, NULL, 0.0f, 0.0f);
#endif

#ifdef __KERNEL_DEBUG__
		if(state.flag & PATH_RAY_CAMERA) {
			debug_data.num_bvh_traversal_steps += isect.num_traversal_steps;
		}
#endif

#ifdef __VOLUME__
		/* volume attenuation, emission, scatter */
		if(state.volume_stack[0].shader != SHADER_NONE) {
			Ray volume_ray = ray;
			volume_ray.t = (hit)? isect.t: FLT_MAX;
			
			bool heterogeneous = volume_stack_is_heterogeneous(kg, state.volume_stack);

#ifdef __VOLUME_DECOUPLED__
			/* decoupled ray marching only supported on CPU */

			/* cache steps along volume for repeated sampling */
			VolumeSegment volume_segment;
			ShaderData volume_sd;

			shader_setup_from_volume(kg, &volume_sd, &volume_ray, state.bounce, state.transparent_bounce);
			kernel_volume_decoupled_record(kg, &state,
				&volume_ray, &volume_sd, &volume_segment, heterogeneous);

			/* direct light sampling */
			if(volume_segment.closure_flag & SD_SCATTER) {
				volume_segment.sampling_method = volume_stack_sampling_method(kg, state.volume_stack);

				bool all = kernel_data.integrator.sample_all_lights_direct;

				kernel_branched_path_volume_connect_light(kg, rng, &volume_sd,
					throughput, &state, &L, 1.0f, all, &volume_ray, &volume_segment);

				/* indirect light sampling */
				int num_samples = kernel_data.integrator.volume_samples;
				float num_samples_inv = 1.0f/num_samples;

				for(int j = 0; j < num_samples; j++) {
					/* workaround to fix correlation bug in T38710, can find better solution
					 * in random number generator later, for now this is done here to not impact
					 * performance of rendering without volumes */
					RNG tmp_rng = cmj_hash(*rng, state.rng_offset);

					PathState ps = state;
					Ray pray = ray;
					float3 tp = throughput;

					/* branch RNG state */
					path_state_branch(&ps, j, num_samples);

					/* scatter sample. if we use distance sampling and take just one
					 * sample for direct and indirect light, we could share this
					 * computation, but makes code a bit complex */
					float rphase = path_state_rng_1D_for_decision(kg, &tmp_rng, &ps, PRNG_PHASE);
					float rscatter = path_state_rng_1D_for_decision(kg, &tmp_rng, &ps, PRNG_SCATTER_DISTANCE);

					VolumeIntegrateResult result = kernel_volume_decoupled_scatter(kg,
						&ps, &pray, &volume_sd, &tp, rphase, rscatter, &volume_segment, NULL, false);
						
					(void)result;
					kernel_assert(result == VOLUME_PATH_SCATTERED);

					if(kernel_path_volume_bounce(kg, rng, &volume_sd, &tp, &ps, &L, &pray)) {
						kernel_path_indirect(kg, rng, pray, tp*num_samples_inv, num_samples, ps, &L);

						/* for render passes, sum and reset indirect light pass variables
						 * for the next samples */
						path_radiance_sum_indirect(&L);
						path_radiance_reset_indirect(&L);
					}
				}
			}

			/* emission and transmittance */
			if(volume_segment.closure_flag & SD_EMISSION)
				path_radiance_accum_emission(&L, throughput, volume_segment.accum_emission, state.bounce);
			throughput *= volume_segment.accum_transmittance;

			/* free cached steps */
			kernel_volume_decoupled_free(kg, &volume_segment);
#else
			/* GPU: no decoupled ray marching, scatter probalistically */
			int num_samples = kernel_data.integrator.volume_samples;
			float num_samples_inv = 1.0f/num_samples;

			/* todo: we should cache the shader evaluations from stepping
			 * through the volume, for now we redo them multiple times */

			for(int j = 0; j < num_samples; j++) {
				PathState ps = state;
				Ray pray = ray;
				ShaderData volume_sd;
				float3 tp = throughput * num_samples_inv;

				/* branch RNG state */
				path_state_branch(&ps, j, num_samples);

				VolumeIntegrateResult result = kernel_volume_integrate(
					kg, &ps, &volume_sd, &volume_ray, &L, &tp, rng, heterogeneous);
				
#ifdef __VOLUME_SCATTER__
				if(result == VOLUME_PATH_SCATTERED) {
					/* todo: support equiangular, MIS and all light sampling.
					 * alternatively get decoupled ray marching working on the GPU */
					kernel_path_volume_connect_light(kg, rng, &volume_sd, tp, &state, &L);

					if(kernel_path_volume_bounce(kg, rng, &volume_sd, &tp, &ps, &L, &pray)) {
						kernel_path_indirect(kg, rng, pray, tp, num_samples, ps, &L);

						/* for render passes, sum and reset indirect light pass variables
						 * for the next samples */
						path_radiance_sum_indirect(&L);
						path_radiance_reset_indirect(&L);
					}
				}
#endif
			}

			/* todo: avoid this calculation using decoupled ray marching */
			kernel_volume_shadow(kg, &state, &volume_ray, &throughput);
#endif
		}
#endif

		if(!hit) {
			/* eval background shader if nothing hit */
			if(kernel_data.background.transparent) {
				L_transparent += average(throughput);

#ifdef __PASSES__
				if(!(kernel_data.film.pass_flag & PASS_BACKGROUND))
#endif
					break;
			}

#ifdef __BACKGROUND__
			/* sample background shader */
			float3 L_background = indirect_background(kg, &state, &ray);
			path_radiance_accum_background(&L, throughput, L_background, state.bounce);
#endif

			break;
		}

		/* setup shading */
		ShaderData sd;
		shader_setup_from_ray(kg, &sd, &isect, &ray, state.bounce, state.transparent_bounce);
		shader_eval_surface(kg, &sd, 0.0f, state.flag, SHADER_CONTEXT_MAIN);
		shader_merge_closures(&sd);

		/* holdout */
#ifdef __HOLDOUT__
		if(sd.flag & (SD_HOLDOUT|SD_HOLDOUT_MASK)) {
			if(kernel_data.background.transparent) {
				float3 holdout_weight;
				
				if(sd.flag & SD_HOLDOUT_MASK)
					holdout_weight = make_float3(1.0f, 1.0f, 1.0f);
				else
					holdout_weight = shader_holdout_eval(kg, &sd);

				/* any throughput is ok, should all be identical here */
				L_transparent += average(holdout_weight*throughput);
			}

			if(sd.flag & SD_HOLDOUT_MASK)
				break;
		}
#endif

		/* holdout mask objects do not write data passes */
		kernel_write_data_passes(kg, buffer, &L, &sd, sample, &state, throughput);

#ifdef __EMISSION__
		/* emission */
		if(sd.flag & SD_EMISSION) {
			float3 emission = indirect_primitive_emission(kg, &sd, isect.t, state.flag, state.ray_pdf);
			path_radiance_accum_emission(&L, throughput, emission, state.bounce);
		}
#endif

		/* transparency termination */
		if(state.flag & PATH_RAY_TRANSPARENT) {
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
		}

#ifdef __AO__
		/* ambient occlusion */
		if(kernel_data.integrator.use_ambient_occlusion || (sd.flag & SD_AO)) {
			kernel_branched_path_ao(kg, &sd, &L, &state, rng, throughput);
		}
#endif

#ifdef __SUBSURFACE__
		/* bssrdf scatter to a different location on the same object */
		if(sd.flag & SD_BSSRDF) {
			kernel_branched_path_subsurface_scatter(kg, &sd, &L, &state,
			                                        rng, &ray, throughput);
		}
#endif

		if(!(sd.flag & SD_HAS_ONLY_VOLUME)) {
			PathState hit_state = state;

#ifdef __EMISSION__
			/* direct light */
			if(kernel_data.integrator.use_direct_light) {
				bool all = kernel_data.integrator.sample_all_lights_direct;
				kernel_branched_path_surface_connect_light(kg, rng,
					&sd, &hit_state, throughput, 1.0f, &L, all);
			}
#endif

			/* indirect light */
			kernel_branched_path_surface_indirect_light(kg, rng,
				&sd, throughput, 1.0f, &hit_state, &L);

			/* continue in case of transparency */
			throughput *= shader_bsdf_transparency(kg, &sd);

			if(is_zero(throughput))
				break;
		}

		path_state_next(kg, &state, LABEL_TRANSPARENT);
		ray.P = ray_offset(sd.P, -sd.Ng);
		ray.t -= sd.ray_length; /* clipping works through transparent */


#ifdef __RAY_DIFFERENTIALS__
		ray.dP = sd.dP;
		ray.dD.dx = -sd.dI.dx;
		ray.dD.dy = -sd.dI.dy;
#endif

#ifdef __VOLUME__
		/* enter/exit volume */
		kernel_volume_stack_enter_exit(kg, &sd, state.volume_stack);
#endif
	}

	float3 L_sum = path_radiance_clamp_and_sum(kg, &L);

	kernel_write_light_passes(kg, buffer, &L, sample);

#ifdef __KERNEL_DEBUG__
	kernel_write_debug_passes(kg, buffer, &state, &debug_data, sample);
#endif

	return make_float4(L_sum.x, L_sum.y, L_sum.z, 1.0f - L_transparent);
}

#endif

ccl_device_inline void kernel_path_trace_setup(KernelGlobals *kg, ccl_global uint *rng_state, int sample, int x, int y, RNG *rng, Ray *ray)
{
	float filter_u;
	float filter_v;

	int num_samples = kernel_data.integrator.aa_samples;

	path_rng_init(kg, rng_state, sample, num_samples, rng, x, y, &filter_u, &filter_v);

	/* sample camera ray */

	float lens_u = 0.0f, lens_v = 0.0f;

	if(kernel_data.cam.aperturesize > 0.0f)
		path_rng_2D(kg, rng, sample, num_samples, PRNG_LENS_U, &lens_u, &lens_v);

	float time = 0.0f;

#ifdef __CAMERA_MOTION__
	if(kernel_data.cam.shuttertime != -1.0f)
		time = path_rng_1D(kg, rng, sample, num_samples, PRNG_TIME);
#endif

	camera_sample(kg, x, y, filter_u, filter_v, lens_u, lens_v, time, ray);
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
	float4 L;

	if(ray.t != 0.0f)
		L = kernel_path_integrate(kg, &rng, sample, ray, buffer);
	else
		L = make_float4(0.0f, 0.0f, 0.0f, 0.0f);

	/* accumulate result in output buffer */
	kernel_write_pass_float4(buffer, sample, L);

	path_rng_end(kg, rng_state, rng);
}

#ifdef __BRANCHED_PATH__
ccl_device void kernel_branched_path_trace(KernelGlobals *kg,
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
	float4 L;

	if(ray.t != 0.0f)
		L = kernel_branched_path_integrate(kg, &rng, sample, ray, buffer);
	else
		L = make_float4(0.0f, 0.0f, 0.0f, 0.0f);

	/* accumulate result in output buffer */
	kernel_write_pass_float4(buffer, sample, L);

	path_rng_end(kg, rng_state, rng);
}
#endif

CCL_NAMESPACE_END

