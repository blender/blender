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

/* Volume shader properties
 *
 * extinction coefficient = absorption coefficient + scattering coefficient
 * sigma_t = sigma_a + sigma_s */

ccl_device float3 volume_shader_get_extinction_coefficient(ShaderData *sd)
{
	float3 sigma_t = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < sd->num_closure; i++) {
		const ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_VOLUME(sc->type))
			sigma_t += sc->weight;
	}

	return sigma_t;
}

ccl_device float3 volume_shader_get_scattering_coefficient(ShaderData *sd)
{
	float3 sigma_s = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < sd->num_closure; i++) {
		const ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_VOLUME(sc->type) && sc->type != CLOSURE_VOLUME_ABSORPTION_ID)
			sigma_s += sc->weight;
	}

	return sigma_s;
}

ccl_device float3 volume_shader_get_absorption_coefficient(ShaderData *sd)
{
	float3 sigma_a = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < sd->num_closure; i++) {
		const ShaderClosure *sc = &sd->closure[i];

		if(sc->type == CLOSURE_VOLUME_ABSORPTION_ID)
			sigma_a += sc->weight;
	}

	return sigma_a;
}

/* evaluate shader to get extinction coefficient at P */
ccl_device float3 volume_extinction_sample(KernelGlobals *kg, ShaderData *sd, VolumeStack *stack, int path_flag, ShaderContext ctx, float3 P)
{
	sd->P = P;

	shader_eval_volume(kg, sd, stack, 0.0f, path_flag, ctx);

	return volume_shader_get_extinction_coefficient(sd);
}

ccl_device float3 volume_color_attenuation(float3 sigma, float t)
{
	return make_float3(expf(-sigma.x * t), expf(-sigma.y * t), expf(-sigma.z * t));
}

/* Volumetric Shadows */

/* get the volume attenuation over line segment defined by segment_ray, with the
 * assumption that there are surfaces blocking light between the endpoints */
ccl_device float3 kernel_volume_get_shadow_attenuation(KernelGlobals *kg, PathState *state, Ray *segment_ray)
{
	ShaderData sd;
	shader_setup_from_volume(kg, &sd, segment_ray, state->volume_stack[0].shader, state->bounce);

	/* do we have a volume shader? */
	if(!(sd.flag & SD_HAS_VOLUME))
		return make_float3(1.0f, 1.0f, 1.0f);

	/* single shader evaluation at the start */
	ShaderContext ctx = SHADER_CONTEXT_SHADOW;
	int path_flag = PATH_RAY_SHADOW;
	float3 attenuation;

	//if(sd.flag & SD_HOMOGENEOUS_VOLUME) {
		/* homogenous volume: assume shader evaluation at the starts gives
		 * the extinction coefficient for the entire line segment */

		/* todo: could this use sigma_t_cache? */
		float3 sigma_t = volume_extinction_sample(kg, &sd, state->volume_stack, path_flag, ctx, segment_ray->P);

		attenuation = volume_color_attenuation(sigma_t, segment_ray->t);
	//}

	return attenuation;
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

