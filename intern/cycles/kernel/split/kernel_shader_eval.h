/*
 * Copyright 2011-2017 Blender Foundation
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

/* This kernel evaluates ShaderData structure from the values computed
 * by the previous kernels.
 */
ccl_device void kernel_shader_eval(KernelGlobals *kg)
{

	int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	/* Sorting on cuda split is not implemented */
#ifdef __KERNEL_CUDA__
	int queue_index = kernel_split_params.queue_index[QUEUE_ACTIVE_AND_REGENERATED_RAYS];
#else
	int queue_index = kernel_split_params.queue_index[QUEUE_SHADER_SORTED_RAYS];
#endif
	if(ray_index >= queue_index) {
		return;
	}
	ray_index = get_ray_index(kg, ray_index,
#ifdef __KERNEL_CUDA__
	                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
#else
	                          QUEUE_SHADER_SORTED_RAYS,
#endif
	                          kernel_split_state.queue_data,
	                          kernel_split_params.queue_size,
	                          0);

	if(ray_index == QUEUE_EMPTY_SLOT) {
		return;
	}

	ccl_global char *ray_state = kernel_split_state.ray_state;
	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
		RNG rng = kernel_split_state.rng[ray_index];
		ccl_global PathState *state = &kernel_split_state.path_state[ray_index];

#ifndef __BRANCHED_PATH__
		float rbsdf = path_state_rng_1D_for_decision(kg, &rng, state, PRNG_BSDF);
		shader_eval_surface(kg, &kernel_split_state.sd[ray_index], &rng, state, rbsdf, state->flag, SHADER_CONTEXT_MAIN);
#else
		ShaderContext ctx = SHADER_CONTEXT_MAIN;
		float rbsdf = 0.0f;

		if(!kernel_data.integrator.branched || IS_FLAG(ray_state, ray_index, RAY_BRANCHED_INDIRECT)) {
			rbsdf = path_state_rng_1D_for_decision(kg, &rng, state, PRNG_BSDF);

		}

		if(IS_FLAG(ray_state, ray_index, RAY_BRANCHED_INDIRECT)) {
			ctx = SHADER_CONTEXT_INDIRECT;
		}

		shader_eval_surface(kg, &kernel_split_state.sd[ray_index], &rng, state, rbsdf, state->flag, ctx);
		shader_merge_closures(&kernel_split_state.sd[ray_index]);
#endif  /* __BRANCHED_PATH__ */

		kernel_split_state.rng[ray_index] = rng;
	}
}

CCL_NAMESPACE_END
