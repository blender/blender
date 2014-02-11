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

#include "kernel_differential.h"
#include "kernel_montecarlo.h"
#include "kernel_projection.h"
#include "kernel_object.h"
#include "kernel_triangle.h"
#include "kernel_curve.h"
#include "kernel_primitive.h"
#include "kernel_projection.h"
#include "kernel_random.h"
#include "kernel_bvh.h"
#include "kernel_accumulate.h"
#include "kernel_camera.h"
#include "kernel_shader.h"
#include "kernel_light.h"
#include "kernel_emission.h"
#include "kernel_passes.h"

#ifdef __SUBSURFACE__
#include "kernel_subsurface.h"
#endif

#ifdef __VOLUME__
#include "kernel_volume.h"
#endif

#include "kernel_path_state.h"
#include "kernel_shadow.h"

CCL_NAMESPACE_BEGIN

#ifdef __VOLUME__

ccl_device_inline bool kernel_path_integrate_scatter_lighting(KernelGlobals *kg, RNG *rng,
	ShaderData *sd, float3 *throughput, PathState *state, PathRadiance *L, Ray *ray,
	float num_samples_adjust)
{
#ifdef __EMISSION__
	if(kernel_data.integrator.use_direct_light) {
		/* sample illumination from lights to find path contribution */
		if(sd->flag & SD_BSDF_HAS_EVAL) {
			float light_t = path_state_rng_1D(kg, rng, state, PRNG_LIGHT);
#ifdef __MULTI_CLOSURE__
			float light_o = 0.0f;
#else
			float light_o = path_state_rng_1D(kg, rng, state, PRNG_LIGHT_F);
#endif
			float light_u, light_v;
			path_state_rng_2D(kg, rng, state, PRNG_LIGHT_U, &light_u, &light_v);

			Ray light_ray;
			BsdfEval L_light;
			bool is_lamp;

#ifdef __OBJECT_MOTION__
			light_ray.time = sd->time;
#endif

			if(direct_emission(kg, sd, -1, light_t, light_o, light_u, light_v, &light_ray, &L_light, &is_lamp, state->bounce)) {
				/* trace shadow ray */
				float3 shadow;

				if(!shadow_blocked(kg, state, &light_ray, &shadow)) {
					/* accumulate */
					path_radiance_accum_light(L, *throughput * num_samples_adjust, &L_light, shadow, 1.0f, state->bounce, is_lamp);
				}
			}
		}
	}
#endif

	/* sample phase function */
	float phase_pdf;
	BsdfEval phase_eval;
	float3 phase_omega_in;
	differential3 phase_domega_in;
	float phase_u, phase_v;
	path_state_rng_2D(kg, rng, state, PRNG_PHASE_U, &phase_u, &phase_v);
	int label;

	label = shader_volume_phase_sample(kg, sd, phase_u, phase_v, &phase_eval,
		&phase_omega_in, &phase_domega_in, &phase_pdf);

	if(phase_pdf == 0.0f || bsdf_eval_is_zero(&phase_eval))
		return false;
	
	/* modify throughput */
	path_radiance_bsdf_bounce(L, throughput, &phase_eval, phase_pdf, state->bounce, label);

	/* set labels */
	state->ray_pdf = phase_pdf;
#ifdef __LAMP_MIS__
	state->ray_t = 0.0f;
#endif
	state->min_ray_pdf = fminf(phase_pdf, state->min_ray_pdf);

	/* update path state */
	path_state_next(kg, state, label);

	/* setup ray */
	ray->P = sd->P;
	ray->D = phase_omega_in;
	ray->t = FLT_MAX;

#ifdef __RAY_DIFFERENTIALS__
	ray->dP = sd->dP;
	ray->dD = phase_domega_in;
#endif

	return true;
}

#endif

#if defined(__BRANCHED_PATH__) || defined(__SUBSURFACE__)

ccl_device void kernel_path_indirect(KernelGlobals *kg, RNG *rng, Ray ray, ccl_global float *buffer,
	float3 throughput, int num_samples, PathState state, PathRadiance *L)
{
	/* path iteration */
	for(;;) {
		/* intersect scene */
		Intersection isect;
		uint visibility = path_state_ray_visibility(kg, &state);
#ifdef __HAIR__
		bool hit = scene_intersect(kg, &ray, visibility, &isect, NULL, 0.0f, 0.0f);
#else
		bool hit = scene_intersect(kg, &ray, visibility, &isect);
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
			float light_t = path_state_rng_1D(kg, rng, &state, PRNG_LIGHT);
			float3 emission;

			if(indirect_lamp_emission(kg, &light_ray, state.flag, state.ray_pdf, light_t, &emission, state.bounce))
				path_radiance_accum_emission(L, throughput, emission, state.bounce);
		}
#endif

#ifdef __VOLUME__
		/* volume attenuation, emission, scatter */
		if(state.volume_stack[0].shader != SHADER_NO_ID) {
			Ray volume_ray = ray;
			volume_ray.t = (hit)? isect.t: FLT_MAX;

			ShaderData volume_sd;
			VolumeIntegrateResult result = kernel_volume_integrate(kg, &state,
				&volume_sd, &volume_ray, L, &throughput, rng);

			if(result == VOLUME_PATH_SCATTERED) {
				if(kernel_path_integrate_scatter_lighting(kg, rng, &volume_sd, &throughput, &state, L, &ray, 1.0f))
					continue;
				else
					break;
			}
		}
#endif

		if(!hit) {
#ifdef __BACKGROUND__
			/* sample background shader */
			float3 L_background = indirect_background(kg, &ray, state.flag, state.ray_pdf, state.bounce);
			path_radiance_accum_background(L, throughput, L_background, state.bounce);
#endif

			break;
		}

		/* setup shading */
		ShaderData sd;
		shader_setup_from_ray(kg, &sd, &isect, &ray, state.bounce);
		float rbsdf = path_state_rng_1D(kg, rng, &state, PRNG_BSDF);
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
			float terminate = path_state_rng_1D(kg, rng, &state, PRNG_TERMINATE);

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

#ifdef __EMISSION__
		if(kernel_data.integrator.use_direct_light) {
			/* sample illumination from lights to find path contribution */
			if(sd.flag & SD_BSDF_HAS_EVAL) {
				float light_t = path_state_rng_1D(kg, rng, &state, PRNG_LIGHT);
#ifdef __MULTI_CLOSURE__
				float light_o = 0.0f;
#else
				float light_o = path_state_rng_1D(kg, rng, &state, PRNG_LIGHT_F);
#endif
				float light_u, light_v;
				path_state_rng_2D(kg, rng, &state, PRNG_LIGHT_U, &light_u, &light_v);

				Ray light_ray;
				BsdfEval L_light;
				bool is_lamp;

#ifdef __OBJECT_MOTION__
				light_ray.time = sd.time;
#endif

				/* sample random light */
				if(direct_emission(kg, &sd, -1, light_t, light_o, light_u, light_v, &light_ray, &L_light, &is_lamp, state.bounce)) {
					/* trace shadow ray */
					float3 shadow;

					if(!shadow_blocked(kg, &state, &light_ray, &shadow)) {
						/* accumulate */
						path_radiance_accum_light(L, throughput, &L_light, shadow, 1.0f, state.bounce, is_lamp);
					}
				}
			}
		}
#endif

		/* no BSDF? we can stop here */
		if(sd.flag & SD_BSDF) {
			/* sample BSDF */
			float bsdf_pdf;
			BsdfEval bsdf_eval;
			float3 bsdf_omega_in;
			differential3 bsdf_domega_in;
			float bsdf_u, bsdf_v;
			path_state_rng_2D(kg, rng, &state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);
			int label;

			label = shader_bsdf_sample(kg, &sd, bsdf_u, bsdf_v, &bsdf_eval,
				&bsdf_omega_in, &bsdf_domega_in, &bsdf_pdf);

			if(bsdf_pdf == 0.0f || bsdf_eval_is_zero(&bsdf_eval))
				break;

			/* modify throughput */
			path_radiance_bsdf_bounce(L, &throughput, &bsdf_eval, bsdf_pdf, state.bounce, label);

			/* set labels */
			if(!(label & LABEL_TRANSPARENT)) {
				state.ray_pdf = bsdf_pdf;
#ifdef __LAMP_MIS__
				state.ray_t = 0.0f;
#endif
				state.min_ray_pdf = fminf(bsdf_pdf, state.min_ray_pdf);
			}

			/* update path state */
			path_state_next(kg, &state, label);

			/* setup ray */
			ray.P = ray_offset(sd.P, (label & LABEL_TRANSMIT)? -sd.Ng: sd.Ng);
			ray.D = bsdf_omega_in;
			ray.t = FLT_MAX;
#ifdef __RAY_DIFFERENTIALS__
			ray.dP = sd.dP;
			ray.dD = bsdf_domega_in;
#endif

#ifdef __VOLUME__
			/* enter/exit volume */
			if(label & LABEL_TRANSMIT)
				kernel_volume_stack_enter_exit(kg, &sd, state.volume_stack);
#endif
		}
#ifdef __VOLUME__
		else if(sd.flag & SD_HAS_ONLY_VOLUME) {
			/* no surface shader but have a volume shader? act transparent */

			/* update path state, count as transparent */
			path_state_next(kg, &state, LABEL_TRANSPARENT);

			/* setup ray position, direction stays unchanged */
			ray.P = ray_offset(sd.P, -sd.Ng);
#ifdef __RAY_DIFFERENTIALS__
			ray.dP = sd.dP;
#endif

			/* enter/exit volume */
			kernel_volume_stack_enter_exit(kg, &sd, state.volume_stack);
		}
#endif
		else {
			/* no bsdf or volume? we're done */
			break;
		}
	}
}

#endif

#ifdef __SUBSURFACE__

ccl_device_inline bool kernel_path_integrate_lighting(KernelGlobals *kg, RNG *rng,
	ShaderData *sd, float3 *throughput, PathState *state, PathRadiance *L, Ray *ray)
{
#ifdef __EMISSION__
	if(kernel_data.integrator.use_direct_light) {
		/* sample illumination from lights to find path contribution */
		if(sd->flag & SD_BSDF_HAS_EVAL) {
			float light_t = path_state_rng_1D(kg, rng, state, PRNG_LIGHT);
#ifdef __MULTI_CLOSURE__
			float light_o = 0.0f;
#else
			float light_o = path_state_rng_1D(kg, rng, state, PRNG_LIGHT_F);
#endif
			float light_u, light_v;
			path_state_rng_2D(kg, rng, state, PRNG_LIGHT_U, &light_u, &light_v);

			Ray light_ray;
			BsdfEval L_light;
			bool is_lamp;

#ifdef __OBJECT_MOTION__
			light_ray.time = sd->time;
#endif

			if(direct_emission(kg, sd, -1, light_t, light_o, light_u, light_v, &light_ray, &L_light, &is_lamp, state->bounce)) {
				/* trace shadow ray */
				float3 shadow;

				if(!shadow_blocked(kg, state, &light_ray, &shadow)) {
					/* accumulate */
					path_radiance_accum_light(L, *throughput, &L_light, shadow, 1.0f, state->bounce, is_lamp);
				}
			}
		}
	}
#endif

	/* no BSDF? we can stop here */
	if(sd->flag & SD_BSDF) {
		/* sample BSDF */
		float bsdf_pdf;
		BsdfEval bsdf_eval;
		float3 bsdf_omega_in;
		differential3 bsdf_domega_in;
		float bsdf_u, bsdf_v;
		path_state_rng_2D(kg, rng, state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);
		int label;

		label = shader_bsdf_sample(kg, sd, bsdf_u, bsdf_v, &bsdf_eval,
			&bsdf_omega_in, &bsdf_domega_in, &bsdf_pdf);

		if(bsdf_pdf == 0.0f || bsdf_eval_is_zero(&bsdf_eval))
			return false;

		/* modify throughput */
		path_radiance_bsdf_bounce(L, throughput, &bsdf_eval, bsdf_pdf, state->bounce, label);

		/* set labels */
		if(!(label & LABEL_TRANSPARENT)) {
			state->ray_pdf = bsdf_pdf;
#ifdef __LAMP_MIS__
			state->ray_t = 0.0f;
#endif
			state->min_ray_pdf = fminf(bsdf_pdf, state->min_ray_pdf);
		}

		/* update path state */
		path_state_next(kg, state, label);

		/* setup ray */
		ray->P = ray_offset(sd->P, (label & LABEL_TRANSMIT)? -sd->Ng: sd->Ng);
		ray->D = bsdf_omega_in;

		if(state->bounce == 0)
			ray->t -= sd->ray_length; /* clipping works through transparent */
		else
			ray->t = FLT_MAX;

#ifdef __RAY_DIFFERENTIALS__
		ray->dP = sd->dP;
		ray->dD = bsdf_domega_in;
#endif

#ifdef __VOLUME__
		/* enter/exit volume */
		if(label & LABEL_TRANSMIT)
			kernel_volume_stack_enter_exit(kg, sd, state->volume_stack);
#endif
		return true;
	}
#ifdef __VOLUME__
	else if(sd->flag & SD_HAS_ONLY_VOLUME) {
		/* no surface shader but have a volume shader? act transparent */

		/* update path state, count as transparent */
		path_state_next(kg, state, LABEL_TRANSPARENT);

		/* setup ray position, direction stays unchanged */
		ray->P = ray_offset(sd->P, -sd->Ng);
#ifdef __RAY_DIFFERENTIALS__
		ray->dP = sd->dP;
#endif

		/* enter/exit volume */
		kernel_volume_stack_enter_exit(kg, sd, state->volume_stack);
		return true;
	}
#endif
	else {
		/* no bsdf or volume? */
		return false;
	}
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
	path_state_init(kg, &state, rng, sample);

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
		bool hit = scene_intersect(kg, &ray, visibility, &isect);
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
			float light_t = path_state_rng_1D(kg, rng, &state, PRNG_LIGHT);
			float3 emission;

			if(indirect_lamp_emission(kg, &light_ray, state.flag, state.ray_pdf, light_t, &emission, state.bounce))
				path_radiance_accum_emission(&L, throughput, emission, state.bounce);
		}
#endif

#ifdef __VOLUME__
		/* volume attenuation, emission, scatter */
		if(state.volume_stack[0].shader != SHADER_NO_ID) {
			Ray volume_ray = ray;
			volume_ray.t = (hit)? isect.t: FLT_MAX;

			ShaderData volume_sd;
			VolumeIntegrateResult result = kernel_volume_integrate(kg, &state,
				&volume_sd, &volume_ray, &L, &throughput, rng);

			if(result == VOLUME_PATH_SCATTERED) {
				if(kernel_path_integrate_scatter_lighting(kg, rng, &volume_sd, &throughput, &state, &L, &ray, 1.0f))
					continue;
				else
					break;
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
			float3 L_background = indirect_background(kg, &ray, state.flag, state.ray_pdf, state.bounce);
			path_radiance_accum_background(&L, throughput, L_background, state.bounce);
#endif

			break;
		}

		/* setup shading */
		ShaderData sd;
		shader_setup_from_ray(kg, &sd, &isect, &ray, state.bounce);
		float rbsdf = path_state_rng_1D(kg, rng, &state, PRNG_BSDF);
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
			float terminate = path_state_rng_1D(kg, rng, &state, PRNG_TERMINATE);

			if(terminate >= probability)
				break;

			throughput /= probability;
		}

#ifdef __AO__
		/* ambient occlusion */
		if(kernel_data.integrator.use_ambient_occlusion || (sd.flag & SD_AO)) {
			/* todo: solve correlation */
			float bsdf_u, bsdf_v;
			path_state_rng_2D(kg, rng, &state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);

			float ao_factor = kernel_data.background.ao_factor;
			float3 ao_N;
			float3 ao_bsdf = shader_bsdf_ao(kg, &sd, ao_factor, &ao_N);
			float3 ao_D;
			float ao_pdf;
			float3 ao_alpha = shader_bsdf_alpha(kg, &sd);

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
					path_radiance_accum_ao(&L, throughput, ao_alpha, ao_bsdf, ao_shadow, state.bounce);
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

				ShaderData bssrdf_sd[BSSRDF_MAX_HITS];
				float bssrdf_u, bssrdf_v;
				path_state_rng_2D(kg, rng, &state, PRNG_BSDF_U, &bssrdf_u, &bssrdf_v);
				int num_hits = subsurface_scatter_multi_step(kg, &sd, bssrdf_sd, state.flag, sc, &lcg_state, bssrdf_u, bssrdf_v, false);

				/* compute lighting with the BSDF closure */
				for(int hit = 0; hit < num_hits; hit++) {
					float3 tp = throughput;
					PathState hit_state = state;
					Ray hit_ray = ray;

					hit_state.flag |= PATH_RAY_BSSRDF_ANCESTOR;
					hit_state.rng_offset += PRNG_BOUNCE_NUM;
					
					if(kernel_path_integrate_lighting(kg, rng, &bssrdf_sd[hit], &tp, &hit_state, &L, &hit_ray)) {
#ifdef __LAMP_MIS__
						hit_state.ray_t = 0.0f;
#endif

						kernel_path_indirect(kg, rng, hit_ray, buffer, tp, state.num_samples, hit_state, &L);

						/* for render passes, sum and reset indirect light pass variables
						 * for the next samples */
						path_radiance_sum_indirect(&L);
						path_radiance_reset_indirect(&L);
					}
				}
				break;
			}
		}
#endif
		
		/* The following code is the same as in kernel_path_integrate_lighting(),
		   but for CUDA the function call is slower. */
#ifdef __EMISSION__
		if(kernel_data.integrator.use_direct_light) {
			/* sample illumination from lights to find path contribution */
			if(sd.flag & SD_BSDF_HAS_EVAL) {
				float light_t = path_state_rng_1D(kg, rng, &state, PRNG_LIGHT);
#ifdef __MULTI_CLOSURE__
				float light_o = 0.0f;
#else
				float light_o = path_state_rng_1D(kg, rng, &state, PRNG_LIGHT_F);
#endif
				float light_u, light_v;
				path_state_rng_2D(kg, rng, &state, PRNG_LIGHT_U, &light_u, &light_v);

				Ray light_ray;
				BsdfEval L_light;
				bool is_lamp;

#ifdef __OBJECT_MOTION__
				light_ray.time = sd.time;
#endif

				if(direct_emission(kg, &sd, -1, light_t, light_o, light_u, light_v, &light_ray, &L_light, &is_lamp, state.bounce)) {
					/* trace shadow ray */
					float3 shadow;

					if(!shadow_blocked(kg, &state, &light_ray, &shadow)) {
						/* accumulate */
						path_radiance_accum_light(&L, throughput, &L_light, shadow, 1.0f, state.bounce, is_lamp);
					}
				}
			}
		}
#endif

		if(sd.flag & SD_BSDF) {
			/* sample BSDF */
			float bsdf_pdf;
			BsdfEval bsdf_eval;
			float3 bsdf_omega_in;
			differential3 bsdf_domega_in;
			float bsdf_u, bsdf_v;
			path_state_rng_2D(kg, rng, &state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);
			int label;

			label = shader_bsdf_sample(kg, &sd, bsdf_u, bsdf_v, &bsdf_eval,
				&bsdf_omega_in, &bsdf_domega_in, &bsdf_pdf);

			if(bsdf_pdf == 0.0f || bsdf_eval_is_zero(&bsdf_eval))
				break;

			/* modify throughput */
			path_radiance_bsdf_bounce(&L, &throughput, &bsdf_eval, bsdf_pdf, state.bounce, label);

			/* set labels */
			if(!(label & LABEL_TRANSPARENT)) {
				state.ray_pdf = bsdf_pdf;
#ifdef __LAMP_MIS__
				state.ray_t = 0.0f;
#endif
				state.min_ray_pdf = fminf(bsdf_pdf, state.min_ray_pdf);
			}

			/* update path state */
			path_state_next(kg, &state, label);

			/* setup ray */
			ray.P = ray_offset(sd.P, (label & LABEL_TRANSMIT)? -sd.Ng: sd.Ng);
			ray.D = bsdf_omega_in;

#ifdef __RAY_DIFFERENTIALS__
			ray.dP = sd.dP;
			ray.dD = bsdf_domega_in;
#endif

#ifdef __VOLUME__
			/* enter/exit volume */
			if(label & LABEL_TRANSMIT)
				kernel_volume_stack_enter_exit(kg, &sd, state.volume_stack);
#endif

		}
#ifdef __VOLUME__
		else if(sd.flag & SD_HAS_ONLY_VOLUME) {
			/* no surface shader but have a volume shader? act transparent */

			/* update path state, count as transparent */
			path_state_next(kg, &state, LABEL_TRANSPARENT);

			/* setup ray position, direction stays unchanged */
			ray.P = ray_offset(sd.P, -sd.Ng);
#ifdef __RAY_DIFFERENTIALS__
			ray.dP = sd.dP;
#endif

			/* enter/exit volume */
			kernel_volume_stack_enter_exit(kg, &sd, state.volume_stack);
		}
#endif
		else {
			/* no bsdf or volume? we're done */
			break;
		}

		/* adjust ray distance for clipping */
		if(state.bounce == 0)
			ray.t -= sd.ray_length; /* clipping works through transparent */
		else
			ray.t = FLT_MAX;
	}

	float3 L_sum = path_radiance_clamp_and_sum(kg, &L);

	kernel_write_light_passes(kg, buffer, &L, sample);

	return make_float4(L_sum.x, L_sum.y, L_sum.z, 1.0f - L_transparent);
}

#ifdef __BRANCHED_PATH__

ccl_device_noinline void kernel_branched_path_integrate_lighting(KernelGlobals *kg,
	RNG *rng, ShaderData *sd, float3 throughput, float num_samples_adjust,
	PathState *state, PathRadiance *L, ccl_global float *buffer)
{
#ifdef __EMISSION__
	/* sample illumination from lights to find path contribution */
	if(sd->flag & SD_BSDF_HAS_EVAL) {
		Ray light_ray;
		BsdfEval L_light;
		bool is_lamp;

#ifdef __OBJECT_MOTION__
		light_ray.time = sd->time;
#endif

		/* lamp sampling */
		for(int i = 0; i < kernel_data.integrator.num_all_lights; i++) {
			int num_samples = ceil_to_int(num_samples_adjust*light_select_num_samples(kg, i));
			float num_samples_inv = num_samples_adjust/(num_samples*kernel_data.integrator.num_all_lights);
			RNG lamp_rng = cmj_hash(*rng, i);

			if(kernel_data.integrator.pdf_triangles != 0.0f)
				num_samples_inv *= 0.5f;

			for(int j = 0; j < num_samples; j++) {
				float light_u, light_v;
				path_branched_rng_2D(kg, &lamp_rng, state, j, num_samples, PRNG_LIGHT_U, &light_u, &light_v);

				if(direct_emission(kg, sd, i, 0.0f, 0.0f, light_u, light_v, &light_ray, &L_light, &is_lamp, state->bounce)) {
					/* trace shadow ray */
					float3 shadow;

					if(!shadow_blocked(kg, state, &light_ray, &shadow)) {
						/* accumulate */
						path_radiance_accum_light(L, throughput*num_samples_inv, &L_light, shadow, num_samples_inv, state->bounce, is_lamp);
					}
				}
			}
		}

		/* mesh light sampling */
		if(kernel_data.integrator.pdf_triangles != 0.0f) {
			int num_samples = ceil_to_int(num_samples_adjust*kernel_data.integrator.mesh_light_samples);
			float num_samples_inv = num_samples_adjust/num_samples;

			if(kernel_data.integrator.num_all_lights)
				num_samples_inv *= 0.5f;

			for(int j = 0; j < num_samples; j++) {
				float light_t = path_branched_rng_1D(kg, rng, state, j, num_samples, PRNG_LIGHT);
				float light_u, light_v;
				path_branched_rng_2D(kg, rng, state, j, num_samples, PRNG_LIGHT_U, &light_u, &light_v);

				/* only sample triangle lights */
				if(kernel_data.integrator.num_all_lights)
					light_t = 0.5f*light_t;

				if(direct_emission(kg, sd, -1, light_t, 0.0f, light_u, light_v, &light_ray, &L_light, &is_lamp, state->bounce)) {
					/* trace shadow ray */
					float3 shadow;

					if(!shadow_blocked(kg, state, &light_ray, &shadow)) {
						/* accumulate */
						path_radiance_accum_light(L, throughput*num_samples_inv, &L_light, shadow, num_samples_inv, state->bounce, is_lamp);
					}
				}
			}
		}
	}
#endif

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
			/* sample BSDF */
			float bsdf_pdf;
			BsdfEval bsdf_eval;
			float3 bsdf_omega_in;
			differential3 bsdf_domega_in;
			float bsdf_u, bsdf_v;
			path_branched_rng_2D(kg, &bsdf_rng, state, j, num_samples, PRNG_BSDF_U, &bsdf_u, &bsdf_v);
			int label;

			label = shader_bsdf_sample_closure(kg, sd, sc, bsdf_u, bsdf_v, &bsdf_eval,
				&bsdf_omega_in, &bsdf_domega_in, &bsdf_pdf);

			if(bsdf_pdf == 0.0f || bsdf_eval_is_zero(&bsdf_eval))
				continue;

			/* modify throughput */
			float3 tp = throughput;
			path_radiance_bsdf_bounce(L, &tp, &bsdf_eval, bsdf_pdf, state->bounce, label);

			/* modify path state */
			PathState ps = *state;
			path_state_next(kg, &ps, label);

			/* setup ray */
			Ray bsdf_ray;

			bsdf_ray.P = ray_offset(sd->P, (label & LABEL_TRANSMIT)? -sd->Ng: sd->Ng);
			bsdf_ray.D = bsdf_omega_in;
			bsdf_ray.t = FLT_MAX;
#ifdef __RAY_DIFFERENTIALS__
			bsdf_ray.dP = sd->dP;
			bsdf_ray.dD = bsdf_domega_in;
#endif
#ifdef __OBJECT_MOTION__
			bsdf_ray.time = sd->time;
#endif

#ifdef __VOLUME__
			/* enter/exit volume */
			if(label & LABEL_TRANSMIT)
				kernel_volume_stack_enter_exit(kg, sd, ps.volume_stack);
#endif

			/* branch RNG state */
			path_state_branch(&ps, j, num_samples);

			/* set MIS state */
			ps.min_ray_pdf = fminf(bsdf_pdf, FLT_MAX);
			ps.ray_pdf = bsdf_pdf;
#ifdef __LAMP_MIS__
			ps.ray_t = 0.0f;
#endif

			kernel_path_indirect(kg, rng, bsdf_ray, buffer, tp*num_samples_inv, num_samples, ps, L);

			/* for render passes, sum and reset indirect light pass variables
			 * for the next samples */
			path_radiance_sum_indirect(L);
			path_radiance_reset_indirect(L);
		}
	}
}

ccl_device float4 kernel_branched_path_integrate(KernelGlobals *kg, RNG *rng, int sample, Ray ray, ccl_global float *buffer)
{
	/* initialize */
	PathRadiance L;
	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
	float L_transparent = 0.0f;

	path_radiance_init(&L, kernel_data.film.use_light_pass);

	PathState state;
	path_state_init(kg, &state, rng, sample);

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
		bool hit = scene_intersect(kg, &ray, visibility, &isect);
#endif

#ifdef __VOLUME__
		/* volume attenuation, emission, scatter */
		if(state.volume_stack[0].shader != SHADER_NO_ID) {
			Ray volume_ray = ray;
			volume_ray.t = (hit)? isect.t: FLT_MAX;

			int num_samples = kernel_data.integrator.volume_samples;
			float num_samples_inv = 1.0f/num_samples;
			float3 avg_tp = make_float3(0.0f, 0.0f, 0.0f);

			/* todo: we should cache the shader evaluations from stepping
			 * through the volume, for now we redo them multiple times */

			for(int j = 0; j < num_samples; j++) {
				PathState ps = state;
				Ray pray = ray;
				ShaderData volume_sd;
				float3 tp = throughput;

				/* branch RNG state */
				path_state_branch(&ps, j, num_samples);

				VolumeIntegrateResult result = kernel_volume_integrate(kg, &ps,
					&volume_sd, &volume_ray, &L, &tp, rng);
				
				if(result == VOLUME_PATH_SCATTERED) {
					/* todo: use all-light sampling */
					if(kernel_path_integrate_scatter_lighting(kg, rng, &volume_sd, &tp, &ps, &L, &pray, num_samples_inv)) {
						kernel_path_indirect(kg, rng, pray, buffer, tp*num_samples_inv, num_samples, ps, &L);

						/* for render passes, sum and reset indirect light pass variables
						 * for the next samples */
						path_radiance_sum_indirect(&L);
						path_radiance_reset_indirect(&L);
					}
				}
				else
					avg_tp += tp;
			}

			throughput = avg_tp * num_samples_inv;
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
			float3 L_background = indirect_background(kg, &ray, state.flag, state.ray_pdf, state.bounce);
			path_radiance_accum_background(&L, throughput, L_background, state.bounce);
#endif

			break;
		}

		/* setup shading */
		ShaderData sd;
		shader_setup_from_ray(kg, &sd, &isect, &ray, state.bounce);
		shader_eval_surface(kg, &sd, 0.0f, state.flag, SHADER_CONTEXT_MAIN);
		shader_merge_closures(&sd);

		/* holdout */
#ifdef __HOLDOUT__
		if((sd.flag & (SD_HOLDOUT|SD_HOLDOUT_MASK))) {
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
				float terminate = path_state_rng_1D(kg, rng, &state, PRNG_TERMINATE);

				if(terminate >= probability)
					break;

				throughput /= probability;
			}
		}

#ifdef __AO__
		/* ambient occlusion */
		if(kernel_data.integrator.use_ambient_occlusion || (sd.flag & SD_AO)) {
			int num_samples = kernel_data.integrator.ao_samples;
			float num_samples_inv = 1.0f/num_samples;
			float ao_factor = kernel_data.background.ao_factor;
			float3 ao_N;
			float3 ao_bsdf = shader_bsdf_ao(kg, &sd, ao_factor, &ao_N);
			float3 ao_alpha = shader_bsdf_alpha(kg, &sd);

			for(int j = 0; j < num_samples; j++) {
				float bsdf_u, bsdf_v;
				path_branched_rng_2D(kg, rng, &state, j, num_samples, PRNG_BSDF_U, &bsdf_u, &bsdf_v);

				float3 ao_D;
				float ao_pdf;

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
						path_radiance_accum_ao(&L, throughput*num_samples_inv, ao_alpha, ao_bsdf, ao_shadow, state.bounce);
				}
			}
		}
#endif

#ifdef __SUBSURFACE__
		/* bssrdf scatter to a different location on the same object */
		if(sd.flag & SD_BSSRDF) {
			for(int i = 0; i< sd.num_closure; i++) {
				ShaderClosure *sc = &sd.closure[i];

				if(!CLOSURE_IS_BSSRDF(sc->type))
					continue;

				/* set up random number generator */
				uint lcg_state = lcg_state_init(rng, &state, 0x68bc21eb);
				int num_samples = kernel_data.integrator.subsurface_samples;
				float num_samples_inv = 1.0f/num_samples;
				RNG bssrdf_rng = cmj_hash(*rng, i);

				state.flag |= PATH_RAY_BSSRDF_ANCESTOR;

				/* do subsurface scatter step with copy of shader data, this will
				 * replace the BSSRDF with a diffuse BSDF closure */
				for(int j = 0; j < num_samples; j++) {
						ShaderData bssrdf_sd[BSSRDF_MAX_HITS];
						float bssrdf_u, bssrdf_v;
						path_branched_rng_2D(kg, &bssrdf_rng, &state, j, num_samples, PRNG_BSDF_U, &bssrdf_u, &bssrdf_v);
						int num_hits = subsurface_scatter_multi_step(kg, &sd, bssrdf_sd, state.flag, sc, &lcg_state, bssrdf_u, bssrdf_v, true);

						/* compute lighting with the BSDF closure */
						for(int hit = 0; hit < num_hits; hit++) {
							PathState hit_state = state;

							path_state_branch(&hit_state, j, num_samples);

							kernel_branched_path_integrate_lighting(kg, rng,
								&bssrdf_sd[hit], throughput, num_samples_inv,
								&hit_state, &L, buffer);
						}
				}

				state.flag &= ~PATH_RAY_BSSRDF_ANCESTOR;
			}
		}
#endif

		if(!(sd.flag & SD_HAS_ONLY_VOLUME)) {
			PathState hit_state = state;

			/* lighting */
			kernel_branched_path_integrate_lighting(kg, rng,
				&sd, throughput, 1.0f, &hit_state, &L, buffer);

			/* continue in case of transparency */
			throughput *= shader_bsdf_transparency(kg, &sd);

			if(is_zero(throughput))
				break;
		}

		path_state_next(kg, &state, LABEL_TRANSPARENT);
		ray.P = ray_offset(sd.P, -sd.Ng);
		ray.t -= sd.ray_length; /* clipping works through transparent */

#ifdef __VOLUME__
		/* enter/exit volume */
		kernel_volume_stack_enter_exit(kg, &sd, state.volume_stack);
#endif
	}

	float3 L_sum = path_radiance_clamp_and_sum(kg, &L);

	kernel_write_light_passes(kg, buffer, &L, sample);

	return make_float4(L_sum.x, L_sum.y, L_sum.z, 1.0f - L_transparent);
}

#endif

ccl_device_inline void kernel_path_trace_setup(KernelGlobals *kg, ccl_global uint *rng_state, int sample, int x, int y, RNG *rng, Ray *ray)
{
	float filter_u;
	float filter_v;
#ifdef __CMJ__
	int num_samples = kernel_data.integrator.aa_samples;
#else
	int num_samples = 0;
#endif

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

