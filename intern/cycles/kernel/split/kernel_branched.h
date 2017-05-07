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

#ifdef __BRANCHED_PATH__

/* sets up the various state needed to do an indirect loop */
ccl_device_inline void kernel_split_branched_path_indirect_loop_init(KernelGlobals *kg, int ray_index)
{
	SplitBranchedState *branched_state = &kernel_split_state.branched_state[ray_index];

	/* save a copy of the state to restore later */
#define BRANCHED_STORE(name) \
		branched_state->name = kernel_split_state.name[ray_index];

	BRANCHED_STORE(path_state);
	BRANCHED_STORE(throughput);
	BRANCHED_STORE(ray);
	BRANCHED_STORE(sd);
	BRANCHED_STORE(isect);
	BRANCHED_STORE(ray_state);

#undef BRANCHED_STORE

	/* set loop counters to intial position */
	branched_state->next_closure = 0;
	branched_state->next_sample = 0;
}

/* ends an indirect loop and restores the previous state */
ccl_device_inline void kernel_split_branched_path_indirect_loop_end(KernelGlobals *kg, int ray_index)
{
	SplitBranchedState *branched_state = &kernel_split_state.branched_state[ray_index];

	/* restore state */
#define BRANCHED_RESTORE(name) \
		kernel_split_state.name[ray_index] = branched_state->name;

	BRANCHED_RESTORE(path_state);
	BRANCHED_RESTORE(throughput);
	BRANCHED_RESTORE(ray);
	BRANCHED_RESTORE(sd);
	BRANCHED_RESTORE(isect);
	BRANCHED_RESTORE(ray_state);

#undef BRANCHED_RESTORE

	/* leave indirect loop */
	REMOVE_RAY_FLAG(kernel_split_state.ray_state, ray_index, RAY_BRANCHED_INDIRECT);
}

/* bounce off surface and integrate indirect light */
ccl_device_noinline bool kernel_split_branched_path_surface_indirect_light_iter(KernelGlobals *kg,
                                                                                int ray_index,
                                                                                float num_samples_adjust,
                                                                                ShaderData *saved_sd,
                                                                                bool reset_path_state)
{
	SplitBranchedState *branched_state = &kernel_split_state.branched_state[ray_index];

	ShaderData *sd = saved_sd;
	RNG rng = kernel_split_state.rng[ray_index];
	PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
	float3 throughput = branched_state->throughput;
	ccl_global PathState *ps = &kernel_split_state.path_state[ray_index];

	float sum_sample_weight = 0.0f;
#ifdef __DENOISING_FEATURES__
	if(ps->denoising_feature_weight > 0.0f) {
		for(int i = 0; i < sd->num_closure; i++) {
			const ShaderClosure *sc = &sd->closure[i];

			/* transparency is not handled here, but in outer loop */
			if(!CLOSURE_IS_BSDF(sc->type) || CLOSURE_IS_BSDF_TRANSPARENT(sc->type)) {
				continue;
			}

			sum_sample_weight += sc->sample_weight;
		}
	}
	else {
		sum_sample_weight = 1.0f;
	}
#endif  /* __DENOISING_FEATURES__ */

	for(int i = branched_state->next_closure; i < sd->num_closure; i++) {
		const ShaderClosure *sc = &sd->closure[i];

		if(!CLOSURE_IS_BSDF(sc->type))
			continue;
		/* transparency is not handled here, but in outer loop */
		if(sc->type == CLOSURE_BSDF_TRANSPARENT_ID)
			continue;

		int num_samples;

		if(CLOSURE_IS_BSDF_DIFFUSE(sc->type))
			num_samples = kernel_data.integrator.diffuse_samples;
		else if(CLOSURE_IS_BSDF_BSSRDF(sc->type))
			num_samples = 1;
		else if(CLOSURE_IS_BSDF_GLOSSY(sc->type))
			num_samples = kernel_data.integrator.glossy_samples;
		else
			num_samples = kernel_data.integrator.transmission_samples;

		num_samples = ceil_to_int(num_samples_adjust*num_samples);

		float num_samples_inv = num_samples_adjust/num_samples;
		RNG bsdf_rng = cmj_hash(rng, i);

		for(int j = branched_state->next_sample; j < num_samples; j++) {
			if(reset_path_state) {
				*ps = branched_state->path_state;
			}

			ccl_global float3 *tp = &kernel_split_state.throughput[ray_index];
			*tp = throughput;

			ccl_global Ray *bsdf_ray = &kernel_split_state.ray[ray_index];

			if(!kernel_branched_path_surface_bounce(kg,
			                                        &bsdf_rng,
			                                        sd,
			                                        sc,
			                                        j,
			                                        num_samples,
			                                        tp,
			                                        ps,
			                                        L,
			                                        bsdf_ray,
			                                        sum_sample_weight))
			{
				continue;
			}

			/* update state for next iteration */
			branched_state->next_closure = i;
			branched_state->next_sample = j+1;
			branched_state->num_samples = num_samples;

			/* start the indirect path */
			*tp *= num_samples_inv;

			return true;
		}

		branched_state->next_sample = 0;
	}

	return false;
}

#endif  /* __BRANCHED_PATH__ */

CCL_NAMESPACE_END

