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

CCL_NAMESPACE_BEGIN

#if defined(__BRANCHED_PATH__) || defined(__SUBSURFACE__) || defined(__SHADOW_TRICKS__) || defined(__BAKING__)
/* branched path tracing: connect path directly to position on one or more lights and add it to L */
ccl_device_noinline void kernel_branched_path_surface_connect_light(
        KernelGlobals *kg,
        ShaderData *sd,
        ShaderData *emission_sd,
        ccl_addr_space PathState *state,
        float3 throughput,
        float num_samples_adjust,
        PathRadiance *L,
        int sample_all_lights)
{
#ifdef __EMISSION__
	/* sample illumination from lights to find path contribution */
	if(!(sd->flag & SD_BSDF_HAS_EVAL))
		return;

	Ray light_ray;
	BsdfEval L_light;
	bool is_lamp;

#  ifdef __OBJECT_MOTION__
	light_ray.time = sd->time;
#  endif

	if(sample_all_lights) {
		/* lamp sampling */
		for(int i = 0; i < kernel_data.integrator.num_all_lights; i++) {
			if(UNLIKELY(light_select_reached_max_bounces(kg, i, state->bounce)))
				continue;

			int num_samples = ceil_to_int(num_samples_adjust*light_select_num_samples(kg, i));
			float num_samples_inv = num_samples_adjust/(num_samples*kernel_data.integrator.num_all_lights);
			uint lamp_rng_hash = cmj_hash(state->rng_hash, i);

			for(int j = 0; j < num_samples; j++) {
				float light_u, light_v;
				path_branched_rng_2D(kg, lamp_rng_hash, state, j, num_samples, PRNG_LIGHT_U, &light_u, &light_v);
				float terminate = path_branched_rng_light_termination(kg, lamp_rng_hash, state, j, num_samples);

				LightSample ls;
				if(lamp_light_sample(kg, i, light_u, light_v, sd->P, &ls)) {
					/* The sampling probability returned by lamp_light_sample assumes that all lights were sampled.
					 * However, this code only samples lamps, so if the scene also had mesh lights, the real probability is twice as high. */
					if(kernel_data.integrator.pdf_triangles != 0.0f)
						ls.pdf *= 2.0f;

					if(direct_emission(kg, sd, emission_sd, &ls, state, &light_ray, &L_light, &is_lamp, terminate)) {
						/* trace shadow ray */
						float3 shadow;

						if(!shadow_blocked(kg, sd, emission_sd, state, &light_ray, &shadow)) {
							/* accumulate */
							path_radiance_accum_light(L, state, throughput*num_samples_inv, &L_light, shadow, num_samples_inv, is_lamp);
						}
						else {
							path_radiance_accum_total_light(L, state, throughput*num_samples_inv, &L_light);
						}
					}
				}
			}
		}

		/* mesh light sampling */
		if(kernel_data.integrator.pdf_triangles != 0.0f) {
			int num_samples = ceil_to_int(num_samples_adjust*kernel_data.integrator.mesh_light_samples);
			float num_samples_inv = num_samples_adjust/num_samples;

			for(int j = 0; j < num_samples; j++) {
				float light_u, light_v;
				path_branched_rng_2D(kg, state->rng_hash, state, j, num_samples, PRNG_LIGHT_U, &light_u, &light_v);
				float terminate = path_branched_rng_light_termination(kg, state->rng_hash, state, j, num_samples);

				/* only sample triangle lights */
				if(kernel_data.integrator.num_all_lights)
					light_u = 0.5f*light_u;

				LightSample ls;
				if(light_sample(kg, light_u, light_v, sd->time, sd->P, state->bounce, &ls)) {
					/* Same as above, probability needs to be corrected since the sampling was forced to select a mesh light. */
					if(kernel_data.integrator.num_all_lights)
						ls.pdf *= 2.0f;

					if(direct_emission(kg, sd, emission_sd, &ls, state, &light_ray, &L_light, &is_lamp, terminate)) {
						/* trace shadow ray */
						float3 shadow;

						if(!shadow_blocked(kg, sd, emission_sd, state, &light_ray, &shadow)) {
							/* accumulate */
							path_radiance_accum_light(L, state, throughput*num_samples_inv, &L_light, shadow, num_samples_inv, is_lamp);
						}
						else {
							path_radiance_accum_total_light(L, state, throughput*num_samples_inv, &L_light);
						}
					}
				}
			}
		}
	}
	else {
		/* sample one light at random */
		float light_u, light_v;
		path_state_rng_2D(kg, state, PRNG_LIGHT_U, &light_u, &light_v);
		float terminate = path_state_rng_light_termination(kg, state);

		LightSample ls;
		if(light_sample(kg, light_u, light_v, sd->time, sd->P, state->bounce, &ls)) {
			/* sample random light */
			if(direct_emission(kg, sd, emission_sd, &ls, state, &light_ray, &L_light, &is_lamp, terminate)) {
				/* trace shadow ray */
				float3 shadow;

				if(!shadow_blocked(kg, sd, emission_sd, state, &light_ray, &shadow)) {
					/* accumulate */
					path_radiance_accum_light(L, state, throughput*num_samples_adjust, &L_light, shadow, num_samples_adjust, is_lamp);
				}
				else {
					path_radiance_accum_total_light(L, state, throughput*num_samples_adjust, &L_light);
				}
			}
		}
	}
#endif
}

/* branched path tracing: bounce off or through surface to with new direction stored in ray */
ccl_device bool kernel_branched_path_surface_bounce(
        KernelGlobals *kg,
        ShaderData *sd,
        const ShaderClosure *sc,
        int sample,
        int num_samples,
        ccl_addr_space float3 *throughput,
        ccl_addr_space PathState *state,
        PathRadianceState *L_state,
        ccl_addr_space Ray *ray,
        float sum_sample_weight)
{
	/* sample BSDF */
	float bsdf_pdf;
	BsdfEval bsdf_eval;
	float3 bsdf_omega_in;
	differential3 bsdf_domega_in;
	float bsdf_u, bsdf_v;
	path_branched_rng_2D(kg, state->rng_hash, state, sample, num_samples, PRNG_BSDF_U, &bsdf_u, &bsdf_v);
	int label;

	label = shader_bsdf_sample_closure(kg, sd, sc, bsdf_u, bsdf_v, &bsdf_eval,
		&bsdf_omega_in, &bsdf_domega_in, &bsdf_pdf);

	if(bsdf_pdf == 0.0f || bsdf_eval_is_zero(&bsdf_eval))
		return false;

	/* modify throughput */
	path_radiance_bsdf_bounce(kg, L_state, throughput, &bsdf_eval, bsdf_pdf, state->bounce, label);

#ifdef __DENOISING_FEATURES__
	state->denoising_feature_weight *= sc->sample_weight / (sum_sample_weight * num_samples);
#endif

	/* modify path state */
	path_state_next(kg, state, label);

	/* setup ray */
	ray->P = ray_offset(sd->P, (label & LABEL_TRANSMIT)? -sd->Ng: sd->Ng);
	ray->D = normalize(bsdf_omega_in);
	ray->t = FLT_MAX;
#ifdef __RAY_DIFFERENTIALS__
	ray->dP = sd->dP;
	ray->dD = bsdf_domega_in;
#endif
#ifdef __OBJECT_MOTION__
	ray->time = sd->time;
#endif

#ifdef __VOLUME__
	/* enter/exit volume */
	if(label & LABEL_TRANSMIT)
		kernel_volume_stack_enter_exit(kg, sd, state->volume_stack);
#endif

	/* branch RNG state */
	path_state_branch(state, sample, num_samples);

	/* set MIS state */
	state->min_ray_pdf = fminf(bsdf_pdf, FLT_MAX);
	state->ray_pdf = bsdf_pdf;
#ifdef __LAMP_MIS__
	state->ray_t = 0.0f;
#endif

	return true;
}

#endif

/* path tracing: connect path directly to position on a light and add it to L */
ccl_device_inline void kernel_path_surface_connect_light(KernelGlobals *kg,
	ShaderData *sd, ShaderData *emission_sd, float3 throughput, ccl_addr_space PathState *state,
	PathRadiance *L)
{
#ifdef __EMISSION__
	if(!(kernel_data.integrator.use_direct_light && (sd->flag & SD_BSDF_HAS_EVAL)))
		return;

#ifdef __SHADOW_TRICKS__
	if(state->flag & PATH_RAY_SHADOW_CATCHER) {
		kernel_branched_path_surface_connect_light(kg,
		                                           sd,
		                                           emission_sd,
		                                           state,
		                                           throughput,
		                                           1.0f,
		                                           L,
		                                           1);
		return;
	}
#endif

	/* sample illumination from lights to find path contribution */
	float light_u, light_v;
	path_state_rng_2D(kg, state, PRNG_LIGHT_U, &light_u, &light_v);

	Ray light_ray;
	BsdfEval L_light;
	bool is_lamp;

#ifdef __OBJECT_MOTION__
	light_ray.time = sd->time;
#endif

	LightSample ls;
	if(light_sample(kg, light_u, light_v, sd->time, sd->P, state->bounce, &ls)) {
		float terminate = path_state_rng_light_termination(kg, state);
		if(direct_emission(kg, sd, emission_sd, &ls, state, &light_ray, &L_light, &is_lamp, terminate)) {
			/* trace shadow ray */
			float3 shadow;

			if(!shadow_blocked(kg, sd, emission_sd, state, &light_ray, &shadow)) {
				/* accumulate */
				path_radiance_accum_light(L, state, throughput, &L_light, shadow, 1.0f, is_lamp);
			}
			else {
				path_radiance_accum_total_light(L, state, throughput, &L_light);
			}
		}
	}
#endif
}

/* path tracing: bounce off or through surface to with new direction stored in ray */
ccl_device bool kernel_path_surface_bounce(KernelGlobals *kg,
                                           ShaderData *sd,
                                           ccl_addr_space float3 *throughput,
                                           ccl_addr_space PathState *state,
                                           PathRadianceState *L_state,
                                           ccl_addr_space Ray *ray)
{
	/* no BSDF? we can stop here */
	if(sd->flag & SD_BSDF) {
		/* sample BSDF */
		float bsdf_pdf;
		BsdfEval bsdf_eval;
		float3 bsdf_omega_in;
		differential3 bsdf_domega_in;
		float bsdf_u, bsdf_v;
		path_state_rng_2D(kg, state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);
		int label;

		label = shader_bsdf_sample(kg, sd, bsdf_u, bsdf_v, &bsdf_eval,
			&bsdf_omega_in, &bsdf_domega_in, &bsdf_pdf);

		if(bsdf_pdf == 0.0f || bsdf_eval_is_zero(&bsdf_eval))
			return false;

		/* modify throughput */
		path_radiance_bsdf_bounce(kg, L_state, throughput, &bsdf_eval, bsdf_pdf, state->bounce, label);

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
		ray->D = normalize(bsdf_omega_in);

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
		if(!path_state_volume_next(kg, state)) {
			return false;
		}

		if(state->bounce == 0)
			ray->t -= sd->ray_length; /* clipping works through transparent */
		else
			ray->t = FLT_MAX;

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

CCL_NAMESPACE_END
