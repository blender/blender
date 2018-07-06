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
	BRANCHED_STORE(isect);
	BRANCHED_STORE(ray_state);

	*kernel_split_sd(branched_state_sd, ray_index) = *kernel_split_sd(sd, ray_index);
	for(int i = 0; i < kernel_split_sd(branched_state_sd, ray_index)->num_closure; i++) {
		kernel_split_sd(branched_state_sd, ray_index)->closure[i] = kernel_split_sd(sd, ray_index)->closure[i];
	}

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
	BRANCHED_RESTORE(isect);
	BRANCHED_RESTORE(ray_state);

	*kernel_split_sd(sd, ray_index) = *kernel_split_sd(branched_state_sd, ray_index);
	for(int i = 0; i < kernel_split_sd(branched_state_sd, ray_index)->num_closure; i++) {
		kernel_split_sd(sd, ray_index)->closure[i] = kernel_split_sd(branched_state_sd, ray_index)->closure[i];
	}

#undef BRANCHED_RESTORE

	/* leave indirect loop */
	REMOVE_RAY_FLAG(kernel_split_state.ray_state, ray_index, RAY_BRANCHED_INDIRECT);
}

ccl_device_inline bool kernel_split_branched_indirect_start_shared(KernelGlobals *kg, int ray_index)
{
	ccl_global char *ray_state = kernel_split_state.ray_state;

	int inactive_ray = dequeue_ray_index(QUEUE_INACTIVE_RAYS,
		kernel_split_state.queue_data, kernel_split_params.queue_size, kernel_split_params.queue_index);

	if(!IS_STATE(ray_state, inactive_ray, RAY_INACTIVE)) {
		return false;
	}

#define SPLIT_DATA_ENTRY(type, name, num) \
		if(num) { \
			kernel_split_state.name[inactive_ray] = kernel_split_state.name[ray_index]; \
		}
	SPLIT_DATA_ENTRIES_BRANCHED_SHARED
#undef SPLIT_DATA_ENTRY

	*kernel_split_sd(sd, inactive_ray) = *kernel_split_sd(sd, ray_index);
	for(int i = 0; i < kernel_split_sd(sd, ray_index)->num_closure; i++) {
		kernel_split_sd(sd, inactive_ray)->closure[i] = kernel_split_sd(sd, ray_index)->closure[i];
	}

	kernel_split_state.branched_state[inactive_ray].shared_sample_count = 0;
	kernel_split_state.branched_state[inactive_ray].original_ray = ray_index;
	kernel_split_state.branched_state[inactive_ray].waiting_on_shared_samples = false;

	PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
	PathRadiance *inactive_L = &kernel_split_state.path_radiance[inactive_ray];

	path_radiance_init(inactive_L, kernel_data.film.use_light_pass);
	path_radiance_copy_indirect(inactive_L, L);

	ray_state[inactive_ray] = RAY_REGENERATED;
	ADD_RAY_FLAG(ray_state, inactive_ray, RAY_BRANCHED_INDIRECT_SHARED);
	ADD_RAY_FLAG(ray_state, inactive_ray, IS_FLAG(ray_state, ray_index, RAY_BRANCHED_INDIRECT));

	atomic_fetch_and_inc_uint32((ccl_global uint*)&kernel_split_state.branched_state[ray_index].shared_sample_count);

	return true;
}

/* bounce off surface and integrate indirect light */
ccl_device_noinline bool kernel_split_branched_path_surface_indirect_light_iter(KernelGlobals *kg,
                                                                                int ray_index,
                                                                                float num_samples_adjust,
                                                                                ShaderData *saved_sd,
                                                                                bool reset_path_state,
                                                                                bool wait_for_shared)
{
	SplitBranchedState *branched_state = &kernel_split_state.branched_state[ray_index];

	ShaderData *sd = saved_sd;
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

		for(int j = branched_state->next_sample; j < num_samples; j++) {
			if(reset_path_state) {
				*ps = branched_state->path_state;
			}

			ps->rng_hash = cmj_hash(branched_state->path_state.rng_hash, i);

			ccl_global float3 *tp = &kernel_split_state.throughput[ray_index];
			*tp = throughput;

			ccl_global Ray *bsdf_ray = &kernel_split_state.ray[ray_index];

			if(!kernel_branched_path_surface_bounce(kg,
			                                        sd,
			                                        sc,
			                                        j,
			                                        num_samples,
			                                        tp,
			                                        ps,
			                                        &L->state,
			                                        bsdf_ray,
			                                        sum_sample_weight))
			{
				continue;
			}

			ps->rng_hash = branched_state->path_state.rng_hash;

			/* update state for next iteration */
			branched_state->next_closure = i;
			branched_state->next_sample = j+1;

			/* start the indirect path */
			*tp *= num_samples_inv;

			if(kernel_split_branched_indirect_start_shared(kg, ray_index)) {
				continue;
			}

			return true;
		}

		branched_state->next_sample = 0;
	}

	branched_state->next_closure = sd->num_closure;

	if(wait_for_shared) {
		branched_state->waiting_on_shared_samples = (branched_state->shared_sample_count > 0);
		if(branched_state->waiting_on_shared_samples) {
			return true;
		}
	}

	return false;
}

#endif  /* __BRANCHED_PATH__ */

CCL_NAMESPACE_END
