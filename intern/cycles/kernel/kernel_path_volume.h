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

#ifdef __VOLUME__

ccl_device_inline void kernel_path_volume_connect_light(KernelGlobals *kg, RNG *rng,
	ShaderData *sd, float3 throughput, PathState *state, PathRadiance *L, float num_samples_adjust)
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
			path_radiance_accum_light(L, throughput * num_samples_adjust, &L_light, shadow, 1.0f, state->bounce, is_lamp);
		}
	}
#endif
}

ccl_device_inline bool kernel_path_volume_bounce(KernelGlobals *kg, RNG *rng,
	ShaderData *sd, float3 *throughput, PathState *state, PathRadiance *L, Ray *ray,
	float num_samples_adjust)
{
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

CCL_NAMESPACE_END

