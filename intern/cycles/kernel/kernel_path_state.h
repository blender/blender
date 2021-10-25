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

ccl_device_inline void path_state_init(KernelGlobals *kg,
                                       ShaderData *stack_sd,
                                       ccl_addr_space PathState *state,
                                       RNG *rng,
                                       int sample,
                                       ccl_addr_space Ray *ray)
{
	state->flag = PATH_RAY_CAMERA|PATH_RAY_MIS_SKIP;

	state->rng_offset = PRNG_BASE_NUM;
	state->sample = sample;
	state->num_samples = kernel_data.integrator.aa_samples;

	state->bounce = 0;
	state->diffuse_bounce = 0;
	state->glossy_bounce = 0;
	state->transmission_bounce = 0;
	state->transparent_bounce = 0;

#ifdef __DENOISING_FEATURES__
	if(kernel_data.film.pass_denoising_data) {
		state->flag |= PATH_RAY_STORE_SHADOW_INFO;
		state->denoising_feature_weight = 1.0f;
	}
	else {
		state->denoising_feature_weight = 0.0f;
	}
#endif  /* __DENOISING_FEATURES__ */

	state->min_ray_pdf = FLT_MAX;
	state->ray_pdf = 0.0f;
#ifdef __LAMP_MIS__
	state->ray_t = 0.0f;
#endif

#ifdef __VOLUME__
	state->volume_bounce = 0;

	if(kernel_data.integrator.use_volumes) {
		/* Initialize volume stack with volume we are inside of. */
		kernel_volume_stack_init(kg, stack_sd, state, ray, state->volume_stack);
		/* Seed RNG for cases where we can't use stratified samples .*/
		state->rng_congruential = lcg_init(*rng + sample*0x51633e2d);
	}
	else {
		state->volume_stack[0].shader = SHADER_NONE;
	}
#endif

#ifdef __SHADOW_TRICKS__
	state->catcher_object = OBJECT_NONE;
#endif
}

ccl_device_inline void path_state_next(KernelGlobals *kg, ccl_addr_space PathState *state, int label)
{
	/* ray through transparent keeps same flags from previous ray and is
	 * not counted as a regular bounce, transparent has separate max */
	if(label & LABEL_TRANSPARENT) {
		state->flag |= PATH_RAY_TRANSPARENT;
		state->transparent_bounce++;

		/* don't increase random number generator offset here, to avoid some
		 * unwanted patterns, see path_state_rng_1D_for_decision */

		if(!kernel_data.integrator.transparent_shadows)
			state->flag |= PATH_RAY_MIS_SKIP;

		return;
	}

	state->bounce++;

#ifdef __VOLUME__
	if(label & LABEL_VOLUME_SCATTER) {
		/* volume scatter */
		state->flag |= PATH_RAY_VOLUME_SCATTER;
		state->flag &= ~(PATH_RAY_REFLECT|PATH_RAY_TRANSMIT|PATH_RAY_CAMERA|PATH_RAY_TRANSPARENT|PATH_RAY_DIFFUSE|PATH_RAY_GLOSSY|PATH_RAY_SINGULAR|PATH_RAY_MIS_SKIP);

		state->volume_bounce++;
	}
	else
#endif
	{
		/* surface reflection/transmission */
		if(label & LABEL_REFLECT) {
			state->flag |= PATH_RAY_REFLECT;
			state->flag &= ~(PATH_RAY_TRANSMIT|PATH_RAY_VOLUME_SCATTER|PATH_RAY_CAMERA|PATH_RAY_TRANSPARENT);

			if(label & LABEL_DIFFUSE)
				state->diffuse_bounce++;
			else
				state->glossy_bounce++;
		}
		else {
			kernel_assert(label & LABEL_TRANSMIT);

			state->flag |= PATH_RAY_TRANSMIT;
			state->flag &= ~(PATH_RAY_REFLECT|PATH_RAY_VOLUME_SCATTER|PATH_RAY_CAMERA|PATH_RAY_TRANSPARENT);

			state->transmission_bounce++;
		}

		/* diffuse/glossy/singular */
		if(label & LABEL_DIFFUSE) {
			state->flag |= PATH_RAY_DIFFUSE|PATH_RAY_DIFFUSE_ANCESTOR;
			state->flag &= ~(PATH_RAY_GLOSSY|PATH_RAY_SINGULAR|PATH_RAY_MIS_SKIP);
		}
		else if(label & LABEL_GLOSSY) {
			state->flag |= PATH_RAY_GLOSSY;
			state->flag &= ~(PATH_RAY_DIFFUSE|PATH_RAY_SINGULAR|PATH_RAY_MIS_SKIP);
		}
		else {
			kernel_assert(label & LABEL_SINGULAR);

			state->flag |= PATH_RAY_GLOSSY|PATH_RAY_SINGULAR|PATH_RAY_MIS_SKIP;
			state->flag &= ~PATH_RAY_DIFFUSE;
		}
	}

	/* random number generator next bounce */
	state->rng_offset += PRNG_BOUNCE_NUM;

#ifdef __DENOISING_FEATURES__
	if((state->denoising_feature_weight == 0.0f) && !(state->flag & PATH_RAY_SHADOW_CATCHER)) {
		state->flag &= ~PATH_RAY_STORE_SHADOW_INFO;
	}
#endif
}

ccl_device_inline uint path_state_ray_visibility(KernelGlobals *kg, PathState *state)
{
	uint flag = state->flag & PATH_RAY_ALL_VISIBILITY;

	/* for visibility, diffuse/glossy are for reflection only */
	if(flag & PATH_RAY_TRANSMIT)
		flag &= ~(PATH_RAY_DIFFUSE|PATH_RAY_GLOSSY);
	/* todo: this is not supported as its own ray visibility yet */
	if(state->flag & PATH_RAY_VOLUME_SCATTER)
		flag |= PATH_RAY_DIFFUSE;

	return flag;
}

ccl_device_inline float path_state_terminate_probability(KernelGlobals *kg, ccl_addr_space PathState *state, const float3 throughput)
{
	if(state->flag & PATH_RAY_TRANSPARENT) {
		/* transparent rays treated separately */
		if(state->transparent_bounce >= kernel_data.integrator.transparent_max_bounce)
			return 0.0f;
		else if(state->transparent_bounce <= kernel_data.integrator.transparent_min_bounce)
			return 1.0f;
	}
	else {
		/* other rays */
		if((state->bounce >= kernel_data.integrator.max_bounce) ||
		   (state->diffuse_bounce >= kernel_data.integrator.max_diffuse_bounce) ||
		   (state->glossy_bounce >= kernel_data.integrator.max_glossy_bounce) ||
#ifdef __VOLUME__
		   (state->volume_bounce >= kernel_data.integrator.max_volume_bounce) ||
#endif
		   (state->transmission_bounce >= kernel_data.integrator.max_transmission_bounce))
		{
			return 0.0f;
		}
		else if(state->bounce <= kernel_data.integrator.min_bounce) {
			return 1.0f;
		}
	}

	/* probalistic termination */
	return min(average(throughput), 1.0f); /* todo: try using max here */
}

/* TODO(DingTo): Find more meaningful name for this */
ccl_device_inline void path_state_modify_bounce(ccl_addr_space PathState *state, bool increase)
{
	/* Modify bounce temporarily for shader eval */
	if(increase)
		state->bounce += 1;
	else
		state->bounce -= 1;
}

CCL_NAMESPACE_END

