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

CCL_NAMESPACE_BEGIN

#if defined(__BRANCHED_PATH__) || defined(__SUBSURFACE__)

/* branched path tracing: connect path directly to position on one or more lights and add it to L */
ccl_device void kernel_branched_path_surface_connect_light(KernelGlobals *kg, RNG *rng,
	ShaderData *sd, PathState *state, float3 throughput, float num_samples_adjust, PathRadiance *L, bool sample_all_lights)
{
#ifdef __EMISSION__
	/* sample illumination from lights to find path contribution */
	if(!(sd->flag & SD_BSDF_HAS_EVAL))
		return;

	Ray light_ray;
	BsdfEval L_light;
	bool is_lamp;

#ifdef __OBJECT_MOTION__
	light_ray.time = sd->time;
#endif

	if(sample_all_lights) {
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

				LightSample ls;
				light_select(kg, i, light_u, light_v, sd->P, &ls);

				if(direct_emission(kg, sd, &ls, &light_ray, &L_light, &is_lamp, state->bounce, state->transparent_bounce)) {
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

				LightSample ls;
				light_sample(kg, light_t, light_u, light_v, sd->time, sd->P, &ls);

				if(direct_emission(kg, sd, &ls, &light_ray, &L_light, &is_lamp, state->bounce, state->transparent_bounce)) {
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
	else {
		/* sample one light at random */
		float light_t = path_state_rng_1D(kg, rng, state, PRNG_LIGHT);
		float light_u, light_v;
		path_state_rng_2D(kg, rng, state, PRNG_LIGHT_U, &light_u, &light_v);

		LightSample ls;
		light_sample(kg, light_t, light_u, light_v, sd->time, sd->P, &ls);

		/* sample random light */
		if(direct_emission(kg, sd, &ls, &light_ray, &L_light, &is_lamp, state->bounce, state->transparent_bounce)) {
			/* trace shadow ray */
			float3 shadow;

			if(!shadow_blocked(kg, state, &light_ray, &shadow)) {
				/* accumulate */
				path_radiance_accum_light(L, throughput*num_samples_adjust, &L_light, shadow, num_samples_adjust, state->bounce, is_lamp);
			}
		}
	}
#endif
}

/* branched path tracing: bounce off or through surface to with new direction stored in ray */
ccl_device bool kernel_branched_path_surface_bounce(KernelGlobals *kg, RNG *rng,
	ShaderData *sd, const ShaderClosure *sc, int sample, int num_samples,
	float3 *throughput, PathState *state, PathRadiance *L, Ray *ray)
{
	/* sample BSDF */
	float bsdf_pdf;
	BsdfEval bsdf_eval;
	float3 bsdf_omega_in;
	differential3 bsdf_domega_in;
	float bsdf_u, bsdf_v;
	path_branched_rng_2D(kg, rng, state, sample, num_samples, PRNG_BSDF_U, &bsdf_u, &bsdf_v);
	int label;

	label = shader_bsdf_sample_closure(kg, sd, sc, bsdf_u, bsdf_v, &bsdf_eval,
		&bsdf_omega_in, &bsdf_domega_in, &bsdf_pdf);

	if(bsdf_pdf == 0.0f || bsdf_eval_is_zero(&bsdf_eval))
		return false;

	/* modify throughput */
	path_radiance_bsdf_bounce(L, throughput, &bsdf_eval, bsdf_pdf, state->bounce, label);

	/* modify path state */
	path_state_next(kg, state, label);

	/* setup ray */
	ray->P = ray_offset(sd->P, (label & LABEL_TRANSMIT)? -sd->Ng: sd->Ng);
	ray->D = bsdf_omega_in;
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
ccl_device_inline void kernel_path_surface_connect_light(KernelGlobals *kg, RNG *rng,
	ShaderData *sd, float3 throughput, PathState *state, PathRadiance *L)
{
#ifdef __EMISSION__
	if(!(kernel_data.integrator.use_direct_light && (sd->flag & SD_BSDF_HAS_EVAL)))
		return;

	/* sample illumination from lights to find path contribution */
	float light_t = path_state_rng_1D(kg, rng, state, PRNG_LIGHT);
	float light_u, light_v;
	path_state_rng_2D(kg, rng, state, PRNG_LIGHT_U, &light_u, &light_v);

	Ray light_ray;
	BsdfEval L_light;
	bool is_lamp;

#ifdef __OBJECT_MOTION__
	light_ray.time = sd->time;
#endif

	LightSample ls;
	light_sample(kg, light_t, light_u, light_v, sd->time, sd->P, &ls);

	if(direct_emission(kg, sd, &ls, &light_ray, &L_light, &is_lamp, state->bounce, state->transparent_bounce)) {
		/* trace shadow ray */
		float3 shadow;

		if(!shadow_blocked(kg, state, &light_ray, &shadow)) {
			/* accumulate */
			path_radiance_accum_light(L, throughput, &L_light, shadow, 1.0f, state->bounce, is_lamp);
		}
	}
#endif
}

/* path tracing: bounce off or through surface to with new direction stored in ray */
ccl_device_inline bool kernel_path_surface_bounce(KernelGlobals *kg, RNG *rng,
	ShaderData *sd, float3 *throughput, PathState *state, PathRadiance *L, Ray *ray)
{
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

CCL_NAMESPACE_END

