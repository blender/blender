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

typedef enum VolumeIntegrateResult {
	VOLUME_PATH_SCATTERED = 0,
	VOLUME_PATH_ATTENUATED = 1,
	VOLUME_PATH_MISSED = 2
} VolumeIntegrateResult;

/* Volume shader properties
 *
 * extinction coefficient = absorption coefficient + scattering coefficient
 * sigma_t = sigma_a + sigma_s */

typedef struct VolumeShaderCoefficients {
	float3 sigma_a;
	float3 sigma_s;
	float3 emission;
} VolumeShaderCoefficients;

/* evaluate shader to get extinction coefficient at P */
ccl_device bool volume_shader_extinction_sample(KernelGlobals *kg, ShaderData *sd, PathState *state, float3 P, float3 *extinction)
{
	sd->P = P;
	shader_eval_volume(kg, sd, state->volume_stack, PATH_RAY_SHADOW, SHADER_CONTEXT_SHADOW);

	if(!(sd->flag & (SD_ABSORPTION|SD_SCATTER)))
		return false;

	float3 sigma_t = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < sd->num_closure; i++) {
		const ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_VOLUME(sc->type))
			sigma_t += sc->weight;
	}

	*extinction = sigma_t;
	return true;
}

/* evaluate shader to get absorption, scattering and emission at P */
ccl_device bool volume_shader_sample(KernelGlobals *kg, ShaderData *sd, PathState *state, float3 P, VolumeShaderCoefficients *coeff)
{
	sd->P = P;
	shader_eval_volume(kg, sd, state->volume_stack, state->flag, SHADER_CONTEXT_VOLUME);

	if(!(sd->flag & (SD_ABSORPTION|SD_SCATTER|SD_EMISSION)))
		return false;
	
	coeff->sigma_a = make_float3(0.0f, 0.0f, 0.0f);
	coeff->sigma_s = make_float3(0.0f, 0.0f, 0.0f);
	coeff->emission = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < sd->num_closure; i++) {
		const ShaderClosure *sc = &sd->closure[i];

		if(sc->type == CLOSURE_VOLUME_ABSORPTION_ID)
			coeff->sigma_a += sc->weight;
		else if(sc->type == CLOSURE_EMISSION_ID)
			coeff->emission += sc->weight;
		else if(CLOSURE_IS_VOLUME(sc->type))
			coeff->sigma_s += sc->weight;
	}

	/* when at the max number of bounces, treat scattering as absorption */
	if(sd->flag & SD_SCATTER) {
		if(state->volume_bounce >= kernel_data.integrator.max_volume_bounce) {
			coeff->sigma_a += coeff->sigma_s;
			coeff->sigma_s = make_float3(0.0f, 0.0f, 0.0f);
			sd->flag &= ~SD_SCATTER;
			sd->flag |= SD_ABSORPTION;
		}
	}

	return true;
}

ccl_device float3 volume_color_attenuation(float3 sigma, float t)
{
	return make_float3(expf(-sigma.x * t), expf(-sigma.y * t), expf(-sigma.z * t));
}

ccl_device bool volume_stack_is_heterogeneous(KernelGlobals *kg, VolumeStack *stack)
{
	for(int i = 0; stack[i].shader != SHADER_NO_ID; i++) {
		int shader_flag = kernel_tex_fetch(__shader_flag, (stack[i].shader & SHADER_MASK)*2);

		if(shader_flag & SD_HETEROGENEOUS_VOLUME)
			return true;
	}

	return false;
}

/* Volume Shadows
 *
 * These functions are used to attenuate shadow rays to lights. Both absorption
 * and scattering will block light, represented by the extinction coefficient. */

/* homogenous volume: assume shader evaluation at the starts gives
 * the extinction coefficient for the entire line segment */
ccl_device void kernel_volume_shadow_homogeneous(KernelGlobals *kg, PathState *state, Ray *ray, ShaderData *sd, float3 *throughput)
{
	float3 sigma_t;

	if(volume_shader_extinction_sample(kg, sd, state, ray->P, &sigma_t))
		*throughput *= volume_color_attenuation(sigma_t, ray->t);
}

/* heterogeneous volume: integrate stepping through the volume until we
 * reach the end, get absorbed entirely, or run out of iterations */
ccl_device void kernel_volume_shadow_heterogeneous(KernelGlobals *kg, PathState *state, Ray *ray, ShaderData *sd, float3 *throughput)
{
	float3 tp = *throughput;
	const float tp_eps = 1e-10f; /* todo: this is likely not the right value */

	/* prepare for stepping */
	int max_steps = kernel_data.integrator.volume_max_steps;
	float step = kernel_data.integrator.volume_step_size;
	float random_jitter_offset = lcg_step_float(&state->rng_congruential) * step;

	/* compute extinction at the start */
	float t = 0.0f;
	float3 P = ray->P;
	float3 sigma_t;

	if(!volume_shader_extinction_sample(kg, sd, state, P, &sigma_t))
		sigma_t = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < max_steps; i++) {
		/* advance to new position */
		float new_t = min(ray->t, t + random_jitter_offset + i * step);
		float3 new_P = ray->P + ray->D * new_t;
		float3 new_sigma_t;

		/* compute attenuation over segment */
		if(volume_shader_extinction_sample(kg, sd, state, new_P, &new_sigma_t)) {
			/* todo: we could avoid computing expf() for each step by summing,
			 * because exp(a)*exp(b) = exp(a+b), but we still want a quick
			 * tp_eps check too */
			tp *= volume_color_attenuation(0.5f*(sigma_t + new_sigma_t), new_t - t);

			/* stop if nearly all light blocked */
			if(tp.x < tp_eps && tp.y < tp_eps && tp.z < tp_eps)
				break;

			sigma_t = new_sigma_t;
		}
		else {
			/* skip empty space */
			sigma_t = make_float3(0.0f, 0.0f, 0.0f);
		}

		/* stop if at the end of the volume */
		t = new_t;
		if(t == ray->t)
			break;
	}

	*throughput = tp;
}

/* get the volume attenuation over line segment defined by ray, with the
 * assumption that there are no surfaces blocking light between the endpoints */
ccl_device_noinline void kernel_volume_shadow(KernelGlobals *kg, PathState *state, Ray *ray, float3 *throughput)
{
	ShaderData sd;
	shader_setup_from_volume(kg, &sd, ray, state->bounce);

	if(volume_stack_is_heterogeneous(kg, state->volume_stack))
		kernel_volume_shadow_heterogeneous(kg, state, ray, &sd, throughput);
	else
		kernel_volume_shadow_homogeneous(kg, state, ray, &sd, throughput);
}

/* Volume Path */

/* homogenous volume: assume shader evaluation at the starts gives
 * the volume shading coefficient for the entire line segment */
ccl_device VolumeIntegrateResult kernel_volume_integrate_homogeneous(KernelGlobals *kg,
	PathState *state, Ray *ray, ShaderData *sd, PathRadiance *L, float3 *throughput,
	RNG *rng)
{
	VolumeShaderCoefficients coeff;

	if(!volume_shader_sample(kg, sd, state, ray->P, &coeff))
		return VOLUME_PATH_MISSED;

	int closure_flag = sd->flag;
	float t = ray->t;
	float3 new_tp;
	float3 transmittance;

	/* randomly scatter, and if we do t is shortened */
	if(closure_flag & SD_SCATTER) {
		float3 sigma_t = coeff.sigma_a + coeff.sigma_s;

		/* set up variables for sampling */
		float rphase = path_state_rng_1D(kg, rng, state, PRNG_PHASE);
		int channel = (int)(rphase*3.0f);
		sd->randb_closure = rphase*3.0f - channel;

		/* pick random color channel, we use the Veach one-sample
		 * model with balance heuristic for the channels */
		float sample_sigma_t;

		if(channel == 0)
			sample_sigma_t = sigma_t.x;
		else if(channel == 1)
			sample_sigma_t = sigma_t.y;
		else
			sample_sigma_t = sigma_t.z;

		/* distance sampling */
		if(kernel_data.integrator.volume_homogeneous_sampling == 0 || !kernel_data.integrator.num_all_lights) { 
			/* xi is [0, 1[ so log(0) should never happen, division by zero is
			 * avoided because sample_sigma_t > 0 when SD_SCATTER is set */
			float xi = path_state_rng_1D(kg, rng, state, PRNG_SCATTER_DISTANCE);
			float sample_t = min(t, -logf(1.0f - xi)/sample_sigma_t);

			transmittance = volume_color_attenuation(sigma_t, sample_t);

			if(sample_t < t) {
				float pdf = dot(sigma_t, transmittance);
				new_tp = *throughput * coeff.sigma_s * transmittance * (3.0f / pdf);
				t = sample_t;
			}
			else {
				float pdf = (transmittance.x + transmittance.y + transmittance.z);
				new_tp = *throughput * transmittance * (3.0f / pdf);
			}
		}
		/* equi-angular sampling */
		else {
			/* decide if we are going to scatter or not, based on sigma_t. this
			 * is not ideal, instead we should perhaps split the path here and
			 * do both, and at least add multiple importance sampling */
			float xi = path_state_rng_1D(kg, rng, state, PRNG_SCATTER_DISTANCE);
			float sample_transmittance = expf(-sample_sigma_t * t);

			if(xi < sample_transmittance) {
				/* no scattering */
				float3 transmittance = volume_color_attenuation(sigma_t, t);
				float pdf = (transmittance.x + transmittance.y + transmittance.z);
				new_tp = *throughput * transmittance * (3.0f / pdf);
			}
			else {
				/* rescale random number so we can reuse it */
				xi = (xi - sample_transmittance)/(1.0f - sample_transmittance);

				/* equi-angular scattering somewhere on segment 0..t */
				/* see "Importance Sampling Techniques for Path Tracing in Participating Media" */

				/* light RNGs */
				float light_t = path_state_rng_1D(kg, rng, state, PRNG_LIGHT);
				float light_u, light_v;
				path_state_rng_2D(kg, rng, state, PRNG_LIGHT_U, &light_u, &light_v);

				/* light sample */
				LightSample ls;
				light_sample(kg, light_t, light_u, light_v, ray->time, ray->P, &ls);
				if(ls.pdf == 0.0f)
					return VOLUME_PATH_MISSED;

				/* sampling */
				float delta = dot((ls.P - ray->P) , ray->D);
				float D = sqrtf(len_squared(ls.P - ray->P) - delta * delta);
				float theta_a = -atan2f(delta, D);
				float theta_b = atan2f(t - delta, D);
				float t_ = D * tan((xi * theta_b) + (1 - xi) * theta_a);

				float pdf = D / ((theta_b - theta_a) * (D * D + t_ * t_));
				float sample_t = min(t, delta + t_);

				transmittance = volume_color_attenuation(sigma_t, sample_t);

				new_tp = *throughput * coeff.sigma_s * transmittance / ((1.0f - sample_transmittance) * pdf);
				t = sample_t;
			}
		}
	}
	else if(closure_flag & SD_ABSORPTION) {
		/* absorption only, no sampling needed */
		transmittance = volume_color_attenuation(coeff.sigma_a, t);
		new_tp = *throughput * transmittance;
	}

	/* integrate emission attenuated by extinction
	 * integral E * exp(-sigma_t * t) from 0 to t = E * (1 - exp(-sigma_t * t))/sigma_t
	 * this goes to E * t as sigma_t goes to zero
	 *
	 * todo: we should use an epsilon to avoid precision issues near zero sigma_t */
	if(closure_flag & SD_EMISSION) {
		float3 emission = coeff.emission;

		if(closure_flag & SD_ABSORPTION) {
			float3 sigma_t = coeff.sigma_a + coeff.sigma_s;

			emission.x *= (sigma_t.x > 0.0f)? (1.0f - transmittance.x)/sigma_t.x: t;
			emission.y *= (sigma_t.y > 0.0f)? (1.0f - transmittance.y)/sigma_t.y: t;
			emission.z *= (sigma_t.z > 0.0f)? (1.0f - transmittance.z)/sigma_t.z: t;
		}
		else
			emission *= t;

		path_radiance_accum_emission(L, *throughput, emission, state->bounce);
	}

	/* modify throughput */
	if(closure_flag & (SD_ABSORPTION|SD_SCATTER)) {
		*throughput = new_tp;

		/* prepare to scatter to new direction */
		if(t < ray->t) {
			/* adjust throughput and move to new location */
			sd->P = ray->P + t*ray->D;

			return VOLUME_PATH_SCATTERED;
		}
	}

	return VOLUME_PATH_ATTENUATED;
}

/* heterogeneous volume: integrate stepping through the volume until we
 * reach the end, get absorbed entirely, or run out of iterations */
ccl_device VolumeIntegrateResult kernel_volume_integrate_heterogeneous(KernelGlobals *kg,
	PathState *state, Ray *ray, ShaderData *sd, PathRadiance *L, float3 *throughput, RNG *rng)
{
	VolumeShaderCoefficients coeff;
	float3 tp = *throughput;
	const float tp_eps = 1e-10f; /* todo: this is likely not the right value */

	/* prepare for stepping */
	int max_steps = kernel_data.integrator.volume_max_steps;
	float step = kernel_data.integrator.volume_step_size;
	float random_jitter_offset = lcg_step_float(&state->rng_congruential) * step;

	/* compute coefficients at the start */
	float t = 0.0f;
	float3 P = ray->P;

	if(!volume_shader_sample(kg, sd, state, P, &coeff)) {
		coeff.sigma_a = make_float3(0.0f, 0.0f, 0.0f);
		coeff.sigma_s = make_float3(0.0f, 0.0f, 0.0f);
		coeff.emission = make_float3(0.0f, 0.0f, 0.0f);
	}

	/* accumulate these values so we can use a single stratified number to sample */
	float3 accum_transmittance = make_float3(1.0f, 1.0f, 1.0f);
	float3 accum_sigma_t = make_float3(0.0f, 0.0f, 0.0f);
	float3 accum_sigma_s = make_float3(0.0f, 0.0f, 0.0f);

	/* cache some constant variables */
	float nlogxi;
	int channel = -1;
	bool has_scatter = false;

	for(int i = 0; i < max_steps; i++) {
		/* advance to new position */
		float new_t = min(ray->t, t + random_jitter_offset + i * step);
		float3 new_P = ray->P + ray->D * new_t;
		VolumeShaderCoefficients new_coeff;

		/* compute segment */
		if(volume_shader_sample(kg, sd, state, new_P, &new_coeff)) {
			int closure_flag = sd->flag;
			float dt = new_t - t;
			float3 new_tp;
			float3 transmittance;
			bool scatter = false;

			/* randomly scatter, and if we do dt and new_t are shortened */
			if((closure_flag & SD_SCATTER) || (has_scatter && (closure_flag & SD_ABSORPTION))) {
				has_scatter = true;

				/* average sigma_t and sigma_s over segment */
				float3 last_sigma_t = coeff.sigma_a + coeff.sigma_s;
				float3 new_sigma_t = new_coeff.sigma_a + new_coeff.sigma_s;
				float3 sigma_t = 0.5f*(last_sigma_t + new_sigma_t);
				float3 sigma_s = 0.5f*(coeff.sigma_s + new_coeff.sigma_s);

				/* lazily set up variables for sampling */
				if(channel == -1) {
					float xi = path_state_rng_1D(kg, rng, state, PRNG_SCATTER_DISTANCE);
					nlogxi = -logf(1.0f - xi);

					float rphase = path_state_rng_1D(kg, rng, state, PRNG_PHASE);
					channel = (int)(rphase*3.0f);
					sd->randb_closure = rphase*3.0f - channel;
				}

				/* pick random color channel, we use the Veach one-sample
				 * model with balance heuristic for the channels */
				float sample_sigma_t;

				if(channel == 0)
					sample_sigma_t = accum_sigma_t.x + dt*sigma_t.x;
				else if(channel == 1)
					sample_sigma_t = accum_sigma_t.y + dt*sigma_t.y;
				else
					sample_sigma_t = accum_sigma_t.z + dt*sigma_t.z;

				if(nlogxi < sample_sigma_t) {
					/* compute sampling distance */
					sample_sigma_t /= new_t;
					new_t = nlogxi/sample_sigma_t;
					dt = new_t - t;

					transmittance = volume_color_attenuation(sigma_t, dt);

					accum_transmittance *= transmittance;
					accum_sigma_t = (accum_sigma_t + dt*sigma_t)/new_t;
					accum_sigma_s = (accum_sigma_s + dt*sigma_s)/new_t;

					/* todo: it's not clear to me that this is correct if we move
					 * through a color volumed, needs verification */
					float pdf = dot(accum_sigma_t, accum_transmittance);
					new_tp = tp * accum_sigma_s * transmittance * (3.0f / pdf);

					scatter = true;
				}
				else {
					transmittance = volume_color_attenuation(sigma_t, dt);

					accum_transmittance *= transmittance;
					accum_sigma_t += dt*sigma_t;
					accum_sigma_s += dt*sigma_s;

					new_tp = tp * transmittance;
				}
			}
			else if(closure_flag & SD_ABSORPTION) {
				/* absorption only, no sampling needed */
				float3 sigma_a = 0.5f*(coeff.sigma_a + new_coeff.sigma_a);
				transmittance = volume_color_attenuation(sigma_a, dt);

				accum_transmittance *= transmittance;
				accum_sigma_t += dt*sigma_a;

				new_tp = tp * transmittance;

				/* todo: we could avoid computing expf() for each step by summing,
				 * because exp(a)*exp(b) = exp(a+b), but we still want a quick
				 * tp_eps check too */
			}

			/* integrate emission attenuated by absorption 
			 * integral E * exp(-sigma_t * t) from 0 to t = E * (1 - exp(-sigma_t * t))/sigma_t
			 * this goes to E * t as sigma_t goes to zero
			 *
			 * todo: we should use an epsilon to avoid precision issues near zero sigma_t */
			if(closure_flag & SD_EMISSION) {
				float3 emission = 0.5f*(coeff.emission + new_coeff.emission);

				if(closure_flag & SD_ABSORPTION) {
					float3 sigma_t = 0.5f*(coeff.sigma_a + coeff.sigma_s + new_coeff.sigma_a + new_coeff.sigma_s);

					emission.x *= (sigma_t.x > 0.0f)? (1.0f - transmittance.x)/sigma_t.x: dt;
					emission.y *= (sigma_t.y > 0.0f)? (1.0f - transmittance.y)/sigma_t.y: dt;
					emission.z *= (sigma_t.z > 0.0f)? (1.0f - transmittance.z)/sigma_t.z: dt;
				}
				else
					emission *= dt;

				path_radiance_accum_emission(L, tp, emission, state->bounce);
			}

			/* modify throughput */
			if(closure_flag & (SD_ABSORPTION|SD_SCATTER)) {
				tp = new_tp;

				/* stop if nearly all light blocked */
				if(tp.x < tp_eps && tp.y < tp_eps && tp.z < tp_eps) {
					tp = make_float3(0.0f, 0.0f, 0.0f);
					break;
				}

				/* prepare to scatter to new direction */
				if(scatter) {
					/* adjust throughput and move to new location */
					sd->P = ray->P + new_t*ray->D;
					*throughput = tp;

					return VOLUME_PATH_SCATTERED;
				}
			}

			coeff = new_coeff;
		}
		else {
			/* skip empty space */
			coeff.sigma_a = make_float3(0.0f, 0.0f, 0.0f);
			coeff.sigma_s = make_float3(0.0f, 0.0f, 0.0f);
			coeff.emission = make_float3(0.0f, 0.0f, 0.0f);
		}

		/* stop if at the end of the volume */
		t = new_t;
		if(t == ray->t)
			break;
	}

	/* include pdf for volumes with scattering */
	if(has_scatter) {
		float pdf = (accum_transmittance.x + accum_transmittance.y + accum_transmittance.z);
		if(pdf > 0.0f)
			tp *= (3.0f/pdf);
	}

	*throughput = tp;

	return VOLUME_PATH_ATTENUATED;
}

/* get the volume attenuation and emission over line segment defined by
 * ray, with the assumption that there are no surfaces blocking light
 * between the endpoints */
ccl_device_noinline VolumeIntegrateResult kernel_volume_integrate(KernelGlobals *kg,
	PathState *state, ShaderData *sd, Ray *ray, PathRadiance *L, float3 *throughput, RNG *rng)
{
	shader_setup_from_volume(kg, sd, ray, state->bounce);

	if(volume_stack_is_heterogeneous(kg, state->volume_stack))
		return kernel_volume_integrate_heterogeneous(kg, state, ray, sd, L, throughput, rng);
	else
		return kernel_volume_integrate_homogeneous(kg, state, ray, sd, L, throughput, rng);
}

/* Volume Stack
 *
 * This is an array of object/shared ID's that the current segment of the path
 * is inside of. */

ccl_device void kernel_volume_stack_init(KernelGlobals *kg, VolumeStack *stack)
{
	/* todo: this assumes camera is always in air, need to detect when it isn't */
	if(kernel_data.background.volume_shader == SHADER_NO_ID) {
		stack[0].shader = SHADER_NO_ID;
	}
	else {
		stack[0].shader = kernel_data.background.volume_shader;
		stack[0].object = ~0;
		stack[1].shader = SHADER_NO_ID;
	}
}

ccl_device void kernel_volume_stack_enter_exit(KernelGlobals *kg, ShaderData *sd, VolumeStack *stack)
{
	/* todo: we should have some way for objects to indicate if they want the
	 * world shader to work inside them. excluding it by default is problematic
	 * because non-volume objects can't be assumed to be closed manifolds */

	if(!(sd->flag & SD_HAS_VOLUME))
		return;
	
	if(sd->flag & SD_BACKFACING) {
		/* exit volume object: remove from stack */
		for(int i = 0; stack[i].shader != SHADER_NO_ID; i++) {
			if(stack[i].object == sd->object) {
				/* shift back next stack entries */
				do {
					stack[i] = stack[i+1];
					i++;
				}
				while(stack[i].shader != SHADER_NO_ID);

				return;
			}
		}
	}
	else {
		/* enter volume object: add to stack */
		int i;

		for(i = 0; stack[i].shader != SHADER_NO_ID; i++) {
			/* already in the stack? then we have nothing to do */
			if(stack[i].object == sd->object)
				return;
		}

		/* if we exceed the stack limit, ignore */
		if(i >= VOLUME_STACK_SIZE-1)
			return;

		/* add to the end of the stack */
		stack[i].shader = sd->shader;
		stack[i].object = sd->object;
		stack[i+1].shader = SHADER_NO_ID;
	}
}

CCL_NAMESPACE_END

