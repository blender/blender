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

/* Volumetric Shadows */

/* get the volume attenuation over line segment defined by ray, with the
 * assumption that there are no surfaces blocking light between the endpoints */
ccl_device void kernel_volume_get_shadow_attenuation(KernelGlobals *kg, PathState *state, Ray *ray, float3 *throughput)
{
	ShaderData sd;
	shader_setup_from_volume(kg, &sd, ray, state->bounce);

	ShaderContext ctx = SHADER_CONTEXT_SHADOW;
	int path_flag = PATH_RAY_SHADOW;
	float3 sigma_t;

	/* homogenous volume: assume shader evaluation at the starts gives
	 * the extinction coefficient for the entire line segment */
	if(!volume_shader_extinction_sample(kg, &sd, state->volume_stack, path_flag, ctx, ray->P, &sigma_t))
		return;

	*throughput *= volume_color_attenuation(sigma_t, ray->t);
}

/* Volumetric Path */

/* get the volume attenuation and emission over line segment defined by
 * ray, with the assumption that there are no surfaces blocking light
 * between the endpoints */
ccl_device VolumeIntegrateResult kernel_volume_integrate(KernelGlobals *kg, PathState *state, Ray *ray, PathRadiance *L, float3 *throughput)
{
	ShaderData sd;
	shader_setup_from_volume(kg, &sd, ray, state->bounce);

	ShaderContext ctx = SHADER_CONTEXT_VOLUME;
	int path_flag = PATH_RAY_SHADOW;
	VolumeShaderCoefficients coeff;

	/* homogenous volume: assume shader evaluation at the starts gives
	 * the extinction coefficient for the entire line segment */
	if(!volume_shader_sample(kg, &sd, state->volume_stack, path_flag, ctx, ray->P, &coeff))
		return VOLUME_PATH_MISSED;

	/* todo: in principle the SD_EMISSION, SD_ABSORPTION and SD_SCATTER flags
	 * should ensure that one of the components is > 0 and so no division by
	 * zero occurs, however this needs to be double-checked and tested */
	
	int closure_flag = sd.flag;
	float t = ray->t;

	/* compute attenuation from absorption (+ scattering for now) */
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

/* Volume Stack */

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

