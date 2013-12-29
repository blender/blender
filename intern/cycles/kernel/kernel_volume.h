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
	VOLUME_PATH_TERMINATED = 0,
	VOLUME_PATH_SCATTERED = 1,
	VOLUME_PATH_ATTENUATED = 2,
	VOLUME_PATH_MISSED = 3
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
ccl_device bool volume_shader_extinction_sample(KernelGlobals *kg, ShaderData *sd, VolumeStack *stack, int path_flag, ShaderContext ctx, float3 P, float3 *extinction)
{
	sd->P = P;
	shader_eval_volume(kg, sd, stack, 0.0f, path_flag, ctx);

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
ccl_device bool volume_shader_sample(KernelGlobals *kg, ShaderData *sd, VolumeStack *stack, int path_flag, ShaderContext ctx, float3 P, VolumeShaderCoefficients *sample)
{
	sd->P = P;
	shader_eval_volume(kg, sd, stack, 0.0f, path_flag, ctx);

	if(!(sd->flag & (SD_ABSORPTION|SD_SCATTER|SD_EMISSION)))
		return false;

	sample->sigma_a = make_float3(0.0f, 0.0f, 0.0f);
	sample->sigma_s = make_float3(0.0f, 0.0f, 0.0f);
	sample->emission = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < sd->num_closure; i++) {
		const ShaderClosure *sc = &sd->closure[i];

		if(sc->type == CLOSURE_VOLUME_ABSORPTION_ID)
			sample->sigma_a += sc->weight;
		else if(sc->type == CLOSURE_EMISSION_ID)
			sample->emission += sc->weight;
		else if(CLOSURE_IS_VOLUME(sc->type))
			sample->sigma_s += sc->weight;
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

/* Volumetric Shadows
 *
 * These functions are used to attenuate shadow rays to lights. Both absorption
 * and scattering will block light, represented by the extinction coefficient. */

/* homogenous volume: assume shader evaluation at the starts gives
 * the extinction coefficient for the entire line segment */
ccl_device void kernel_volume_shadow_homogeneous(KernelGlobals *kg, PathState *state, Ray *ray, ShaderData *sd, float3 *throughput)
{
	ShaderContext ctx = SHADER_CONTEXT_SHADOW;
	int path_flag = PATH_RAY_SHADOW;
	float3 sigma_t;

	if(volume_shader_extinction_sample(kg, sd, state->volume_stack, path_flag, ctx, ray->P, &sigma_t))
		*throughput *= volume_color_attenuation(sigma_t, ray->t);
}

/* heterogeneous volume: integrate stepping through the volume until we
 * reach the end, get absorbed entirely, or run out of iterations */
ccl_device void kernel_volume_shadow_heterogeneous(KernelGlobals *kg, PathState *state, Ray *ray, ShaderData *sd, float3 *throughput)
{
	ShaderContext ctx = SHADER_CONTEXT_SHADOW;
	int path_flag = PATH_RAY_SHADOW;
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

	if(!volume_shader_extinction_sample(kg, sd, state->volume_stack, path_flag, ctx, P, &sigma_t))
		sigma_t = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < max_steps; i++) {
		/* advance to new position */
		float new_t = min(ray->t, t + random_jitter_offset + i * step);
		float3 new_P = ray->P + ray->D * new_t;
		float3 new_sigma_t;

		/* compute attenuation over segment */
		if(volume_shader_extinction_sample(kg, sd, state->volume_stack, path_flag, ctx, new_P, &new_sigma_t)) {
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
ccl_device void kernel_volume_shadow(KernelGlobals *kg, PathState *state, Ray *ray, float3 *throughput)
{
	ShaderData sd;
	shader_setup_from_volume(kg, &sd, ray, state->bounce);

	if(volume_stack_is_heterogeneous(kg, state->volume_stack))
		kernel_volume_shadow_heterogeneous(kg, state, ray, &sd, throughput);
	else
		kernel_volume_shadow_homogeneous(kg, state, ray, &sd, throughput);
}

/* Volumetric Path */

/* homogenous volume: assume shader evaluation at the starts gives
 * the volume shading coefficient for the entire line segment */
ccl_device VolumeIntegrateResult kernel_volume_integrate_homogeneous(KernelGlobals *kg, PathState *state, Ray *ray, ShaderData *sd, PathRadiance *L, float3 *throughput)
{
	ShaderContext ctx = SHADER_CONTEXT_VOLUME;
	int path_flag = PATH_RAY_SHADOW;
	VolumeShaderCoefficients coeff;

	if(!volume_shader_sample(kg, sd, state->volume_stack, path_flag, ctx, ray->P, &coeff))
		return VOLUME_PATH_MISSED;

	/* todo: in principle the SD_EMISSION, SD_ABSORPTION and SD_SCATTER flags
	 * should ensure that one of the components is > 0 and so no division by
	 * zero occurs, however this needs to be double-checked and tested */
	
	int closure_flag = sd->flag;
	float t = ray->t;

	/* compute attenuation from absorption */
	float3 attenuation;

	if(closure_flag & SD_ABSORPTION)
		attenuation = volume_color_attenuation(coeff.sigma_a, t);

	/* integrate emission attenuated by absorption
	 * integral E * exp(-sigma_a * t) from 0 to t = E * (1 - exp(-sigma_a * t))/sigma_a
	 * this goes to E * t as sigma_a goes to zero
	 *
	 * todo: we should use an epsilon to avoid precision issues near zero sigma_a */
	if(closure_flag & SD_EMISSION) {
		float3 emission = coeff.emission;

		if(closure_flag & SD_ABSORPTION) {
			float3 sigma_a = coeff.sigma_a;

			emission.x *= (sigma_a.x > 0.0f)? (1.0f - attenuation.x)/sigma_a.x: t;
			emission.y *= (sigma_a.y > 0.0f)? (1.0f - attenuation.y)/sigma_a.y: t;
			emission.z *= (sigma_a.z > 0.0f)? (1.0f - attenuation.z)/sigma_a.z: t;
		}
		else
			emission *= t;

		path_radiance_accum_emission(L, *throughput, emission, state->bounce);
	}

	/* modify throughput */
	if(closure_flag & SD_ABSORPTION)
		*throughput *= attenuation;

	return VOLUME_PATH_ATTENUATED;
}

/* heterogeneous volume: integrate stepping through the volume until we
 * reach the end, get absorbed entirely, or run out of iterations */
ccl_device VolumeIntegrateResult kernel_volume_integrate_heterogeneous(KernelGlobals *kg, PathState *state, Ray *ray, ShaderData *sd, PathRadiance *L, float3 *throughput)
{
	ShaderContext ctx = SHADER_CONTEXT_VOLUME;
	int path_flag = PATH_RAY_SHADOW;
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

	if(!volume_shader_sample(kg, sd, state->volume_stack, path_flag, ctx, P, &coeff)) {
		coeff.sigma_a = make_float3(0.0f, 0.0f, 0.0f);
		coeff.sigma_s = make_float3(0.0f, 0.0f, 0.0f);
		coeff.emission = make_float3(0.0f, 0.0f, 0.0f);
	}

	for(int i = 0; i < max_steps; i++) {
		/* advance to new position */
		float new_t = min(ray->t, t + random_jitter_offset + i * step);
		float3 new_P = ray->P + ray->D * new_t;
		VolumeShaderCoefficients new_coeff;

		/* compute attenuation over segment */
		if(volume_shader_sample(kg, sd, state->volume_stack, path_flag, ctx, new_P, &new_coeff)) {
			int closure_flag = sd->flag;
			float dt = new_t - t;

			/* compute attenuation from absorption */
			float3 attenuation, sigma_a;

			if(closure_flag & SD_ABSORPTION) {
				/* todo: we could avoid computing expf() for each step by summing,
				 * because exp(a)*exp(b) = exp(a+b), but we still want a quick
				 * tp_eps check too */
				sigma_a = 0.5f*(coeff.sigma_a + new_coeff.sigma_a);
				attenuation = volume_color_attenuation(sigma_a, dt);
			}

			/* integrate emission attenuated by absorption 
			 * integral E * exp(-sigma_a * t) from 0 to t = E * (1 - exp(-sigma_a * t))/sigma_a
			 * this goes to E * t as sigma_a goes to zero
			 *
			 * todo: we should use an epsilon to avoid precision issues near zero sigma_a */
			if(closure_flag & SD_EMISSION) {
				float3 emission = 0.5f*(coeff.emission + new_coeff.emission);

				if(closure_flag & SD_ABSORPTION) {
					emission.x *= (sigma_a.x > 0.0f)? (1.0f - attenuation.x)/sigma_a.x: dt;
					emission.y *= (sigma_a.y > 0.0f)? (1.0f - attenuation.y)/sigma_a.y: dt;
					emission.z *= (sigma_a.z > 0.0f)? (1.0f - attenuation.z)/sigma_a.z: dt;
				}
				else
					emission *= t;

				path_radiance_accum_emission(L, tp, emission, state->bounce);
			}

			/* modify throughput */
			if(closure_flag & SD_ABSORPTION) {
				tp *= attenuation;

				/* stop if nearly all light blocked */
				if(tp.x < tp_eps && tp.y < tp_eps && tp.z < tp_eps) {
					tp = make_float3(0.0f, 0.0f, 0.0f);
					break;
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

	*throughput = tp;

	return VOLUME_PATH_ATTENUATED;
}

/* get the volume attenuation and emission over line segment defined by
 * ray, with the assumption that there are no surfaces blocking light
 * between the endpoints */
ccl_device VolumeIntegrateResult kernel_volume_integrate(KernelGlobals *kg, PathState *state, Ray *ray, PathRadiance *L, float3 *throughput)
{
	ShaderData sd;
	shader_setup_from_volume(kg, &sd, ray, state->bounce);

	if(volume_stack_is_heterogeneous(kg, state->volume_stack))
		return kernel_volume_integrate_heterogeneous(kg, state, ray, &sd, L, throughput);
	else
		return kernel_volume_integrate_homogeneous(kg, state, ray, &sd, L, throughput);
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

