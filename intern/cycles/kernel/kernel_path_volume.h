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

#ifdef __VOLUME_SCATTER__

ccl_device_inline void kernel_path_volume_connect_light(
        KernelGlobals *kg,
        ShaderData *sd,
        ShaderData *emission_sd,
        float3 throughput,
        ccl_addr_space PathState *state,
        PathRadiance *L)
{
#ifdef __EMISSION__
	if(!kernel_data.integrator.use_direct_light)
		return;

	/* sample illumination from lights to find path contribution */
	float light_u, light_v;
	path_state_rng_2D(kg, state, PRNG_LIGHT_U, &light_u, &light_v);

	Ray light_ray;
	BsdfEval L_light;
	LightSample ls;
	bool is_lamp;

	/* connect to light from given point where shader has been evaluated */
	light_ray.time = sd->time;

	if(light_sample(kg, light_u, light_v, sd->time, sd->P, state->bounce, &ls))
	{
		float terminate = path_state_rng_light_termination(kg, state);
		if(direct_emission(kg, sd, emission_sd, &ls, state, &light_ray, &L_light, &is_lamp, terminate)) {
			/* trace shadow ray */
			float3 shadow;

			if(!shadow_blocked(kg, sd, emission_sd, state, &light_ray, &shadow)) {
				/* accumulate */
				path_radiance_accum_light(L, state, throughput, &L_light, shadow, 1.0f, is_lamp);
			}
		}
	}
#endif /* __EMISSION__ */
}

#ifdef __KERNEL_GPU__
ccl_device_noinline
#else
ccl_device
#endif
bool kernel_path_volume_bounce(
    KernelGlobals *kg,
    ShaderData *sd,
    ccl_addr_space float3 *throughput,
    ccl_addr_space PathState *state,
    PathRadianceState *L_state,
    ccl_addr_space Ray *ray)
{
	/* sample phase function */
	float phase_pdf;
	BsdfEval phase_eval;
	float3 phase_omega_in;
	differential3 phase_domega_in;
	float phase_u, phase_v;
	path_state_rng_2D(kg, state, PRNG_BSDF_U, &phase_u, &phase_v);
	int label;

	label = shader_volume_phase_sample(kg, sd, phase_u, phase_v, &phase_eval,
		&phase_omega_in, &phase_domega_in, &phase_pdf);

	if(phase_pdf == 0.0f || bsdf_eval_is_zero(&phase_eval))
		return false;

	/* modify throughput */
	path_radiance_bsdf_bounce(kg, L_state, throughput, &phase_eval, phase_pdf, state->bounce, label);

	/* set labels */
	state->ray_pdf = phase_pdf;
#ifdef __LAMP_MIS__
	state->ray_t = 0.0f;
#endif
	state->min_ray_pdf = fminf(phase_pdf, state->min_ray_pdf);

	/* update path state */
	path_state_next(kg, state, label);

	/* Russian roulette termination of volume ray scattering. */
	float probability = path_state_continuation_probability(kg, state, *throughput);

	if(probability == 0.0f) {
		return false;
	}
	else if(probability != 1.0f) {
		/* Use dimension from the previous bounce, has not been used yet. */
		float terminate = path_state_rng_1D(kg, state, PRNG_TERMINATE - PRNG_BOUNCE_NUM);

		if(terminate >= probability) {
			return false;
		}

		*throughput /= probability;
	}

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

#ifndef __SPLIT_KERNEL__
ccl_device void kernel_branched_path_volume_connect_light(
        KernelGlobals *kg,
        ShaderData *sd,
        ShaderData *emission_sd,
        float3 throughput,
        ccl_addr_space PathState *state,
        PathRadiance *L,
        bool sample_all_lights,
        Ray *ray,
        const VolumeSegment *segment)
{
#ifdef __EMISSION__
	if(!kernel_data.integrator.use_direct_light)
		return;

	Ray light_ray;
	BsdfEval L_light;
	bool is_lamp;

	light_ray.time = sd->time;

	if(sample_all_lights) {
		/* lamp sampling */
		for(int i = 0; i < kernel_data.integrator.num_all_lights; i++) {
			if(UNLIKELY(light_select_reached_max_bounces(kg, i, state->bounce)))
				continue;

			int num_samples = light_select_num_samples(kg, i);
			float num_samples_inv = 1.0f/(num_samples*kernel_data.integrator.num_all_lights);
			uint lamp_rng_hash = cmj_hash(state->rng_hash, i);

			for(int j = 0; j < num_samples; j++) {
				/* sample random position on given light */
				float light_u, light_v;
				path_branched_rng_2D(kg, lamp_rng_hash, state, j, num_samples, PRNG_LIGHT_U, &light_u, &light_v);

				LightSample ls;
				lamp_light_sample(kg, i, light_u, light_v, ray->P, &ls);

				float3 tp = throughput;

				/* sample position on volume segment */
				float rphase = path_branched_rng_1D(kg, state->rng_hash, state, j, num_samples, PRNG_PHASE_CHANNEL);
				float rscatter = path_branched_rng_1D(kg, state->rng_hash, state, j, num_samples, PRNG_SCATTER_DISTANCE);

				VolumeIntegrateResult result = kernel_volume_decoupled_scatter(kg,
					state, ray, sd, &tp, rphase, rscatter, segment, (ls.t != FLT_MAX)? &ls.P: NULL, false);

				/* todo: split up light_sample so we don't have to call it again with new position */
				if(result == VOLUME_PATH_SCATTERED &&
				   lamp_light_sample(kg, i, light_u, light_v, sd->P, &ls)) {
					if(kernel_data.integrator.pdf_triangles != 0.0f)
						ls.pdf *= 2.0f;

					float terminate = path_branched_rng_light_termination(kg, state->rng_hash, state, j, num_samples);
					if(direct_emission(kg, sd, emission_sd, &ls, state, &light_ray, &L_light, &is_lamp, terminate)) {
						/* trace shadow ray */
						float3 shadow;

						if(!shadow_blocked(kg, sd, emission_sd, state, &light_ray, &shadow)) {
							/* accumulate */
							path_radiance_accum_light(L, state, tp*num_samples_inv, &L_light, shadow, num_samples_inv, is_lamp);
						}
					}
				}
			}
		}

		/* mesh light sampling */
		if(kernel_data.integrator.pdf_triangles != 0.0f) {
			int num_samples = kernel_data.integrator.mesh_light_samples;
			float num_samples_inv = 1.0f/num_samples;

			for(int j = 0; j < num_samples; j++) {
				/* sample random position on random triangle */
				float light_u, light_v;
				path_branched_rng_2D(kg, state->rng_hash, state, j, num_samples, PRNG_LIGHT_U, &light_u, &light_v);

				/* only sample triangle lights */
				if(kernel_data.integrator.num_all_lights)
					light_u = 0.5f*light_u;

				LightSample ls;
				light_sample(kg, light_u, light_v, sd->time, ray->P, state->bounce, &ls);

				float3 tp = throughput;

				/* sample position on volume segment */
				float rphase = path_branched_rng_1D(kg, state->rng_hash, state, j, num_samples, PRNG_PHASE_CHANNEL);
				float rscatter = path_branched_rng_1D(kg, state->rng_hash, state, j, num_samples, PRNG_SCATTER_DISTANCE);

				VolumeIntegrateResult result = kernel_volume_decoupled_scatter(kg,
					state, ray, sd, &tp, rphase, rscatter, segment, (ls.t != FLT_MAX)? &ls.P: NULL, false);

				/* todo: split up light_sample so we don't have to call it again with new position */
				if(result == VOLUME_PATH_SCATTERED &&
				   light_sample(kg, light_u, light_v, sd->time, sd->P, state->bounce, &ls)) {
					if(kernel_data.integrator.num_all_lights)
						ls.pdf *= 2.0f;

					float terminate = path_branched_rng_light_termination(kg, state->rng_hash, state, j, num_samples);
					if(direct_emission(kg, sd, emission_sd, &ls, state, &light_ray, &L_light, &is_lamp, terminate)) {
						/* trace shadow ray */
						float3 shadow;

						if(!shadow_blocked(kg, sd, emission_sd, state, &light_ray, &shadow)) {
							/* accumulate */
							path_radiance_accum_light(L, state, tp*num_samples_inv, &L_light, shadow, num_samples_inv, is_lamp);
						}
					}
				}
			}
		}
	}
	else {
		/* sample random position on random light */
		float light_u, light_v;
		path_state_rng_2D(kg, state, PRNG_LIGHT_U, &light_u, &light_v);

		LightSample ls;
		light_sample(kg, light_u, light_v, sd->time, ray->P, state->bounce, &ls);

		float3 tp = throughput;

		/* sample position on volume segment */
		float rphase = path_state_rng_1D(kg, state, PRNG_PHASE_CHANNEL);
		float rscatter = path_state_rng_1D(kg, state, PRNG_SCATTER_DISTANCE);

		VolumeIntegrateResult result = kernel_volume_decoupled_scatter(kg,
			state, ray, sd, &tp, rphase, rscatter, segment, (ls.t != FLT_MAX)? &ls.P: NULL, false);

		/* todo: split up light_sample so we don't have to call it again with new position */
		if(result == VOLUME_PATH_SCATTERED &&
		   light_sample(kg, light_u, light_v, sd->time, sd->P, state->bounce, &ls)) {
			/* sample random light */
			float terminate = path_state_rng_light_termination(kg, state);
			if(direct_emission(kg, sd, emission_sd, &ls, state, &light_ray, &L_light, &is_lamp, terminate)) {
				/* trace shadow ray */
				float3 shadow;

				if(!shadow_blocked(kg, sd, emission_sd, state, &light_ray, &shadow)) {
					/* accumulate */
					path_radiance_accum_light(L, state, tp, &L_light, shadow, 1.0f, is_lamp);
				}
			}
		}
	}
#endif /* __EMISSION__ */
}
#endif /* __SPLIT_KERNEL__ */

#endif /* __VOLUME_SCATTER__ */

CCL_NAMESPACE_END
