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

#if defined(__BRANCHED_PATH__) && defined(__SUBSURFACE__)

ccl_device_inline void kernel_split_branched_path_subsurface_indirect_light_init(KernelGlobals *kg, int ray_index)
{
	kernel_split_branched_path_indirect_loop_init(kg, ray_index);

	SplitBranchedState *branched_state = &kernel_split_state.branched_state[ray_index];

	branched_state->ss_next_closure = 0;
	branched_state->ss_next_sample = 0;

	branched_state->num_hits = 0;
	branched_state->next_hit = 0;

	ADD_RAY_FLAG(kernel_split_state.ray_state, ray_index, RAY_BRANCHED_SUBSURFACE_INDIRECT);
}

ccl_device_noinline bool kernel_split_branched_path_subsurface_indirect_light_iter(KernelGlobals *kg, int ray_index)
{
	SplitBranchedState *branched_state = &kernel_split_state.branched_state[ray_index];

	ShaderData *sd = &branched_state->sd;
	RNG rng = kernel_split_state.rng[ray_index];
	PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
	ShaderData *emission_sd = &kernel_split_state.sd_DL_shadow[ray_index];

	for(int i = branched_state->ss_next_closure; i < sd->num_closure; i++) {
		ShaderClosure *sc = &sd->closure[i];

		if(!CLOSURE_IS_BSSRDF(sc->type))
			continue;

		/* set up random number generator */
		if(branched_state->ss_next_sample == 0 && branched_state->next_hit == 0 &&
		   branched_state->next_closure == 0 && branched_state->next_sample == 0)
		{
			branched_state->lcg_state = lcg_state_init(&rng,
			                                           branched_state->path_state.rng_offset,
			                                           branched_state->path_state.sample,
			                                           0x68bc21eb);
		}
		int num_samples = kernel_data.integrator.subsurface_samples;
		float num_samples_inv = 1.0f/num_samples;
		RNG bssrdf_rng = cmj_hash(rng, i);

		/* do subsurface scatter step with copy of shader data, this will
		 * replace the BSSRDF with a diffuse BSDF closure */
		for(int j = branched_state->ss_next_sample; j < num_samples; j++) {
			ccl_global SubsurfaceIntersection *ss_isect = &branched_state->ss_isect;
			float bssrdf_u, bssrdf_v;
			path_branched_rng_2D(kg,
			                     &bssrdf_rng,
			                     &branched_state->path_state,
			                     j,
			                     num_samples,
			                     PRNG_BSDF_U,
			                     &bssrdf_u,
			                     &bssrdf_v);

			/* intersection is expensive so avoid doing multiple times for the same input */
			if(branched_state->next_hit == 0 && branched_state->next_closure == 0 && branched_state->next_sample == 0) {
				RNG lcg_state = branched_state->lcg_state;
				SubsurfaceIntersection ss_isect_private;

				branched_state->num_hits = subsurface_scatter_multi_intersect(kg,
				                                                              &ss_isect_private,
				                                                              sd,
				                                                              sc,
				                                                              &lcg_state,
				                                                              bssrdf_u, bssrdf_v,
				                                                              true);

				branched_state->lcg_state = lcg_state;
				*ss_isect = ss_isect_private;
			}

#ifdef __VOLUME__
			Ray volume_ray = branched_state->ray;
			bool need_update_volume_stack =
			        kernel_data.integrator.use_volumes &&
			        sd->object_flag & SD_OBJECT_INTERSECTS_VOLUME;
#endif  /* __VOLUME__ */

			/* compute lighting with the BSDF closure */
			for(int hit = branched_state->next_hit; hit < branched_state->num_hits; hit++) {
				ShaderData *bssrdf_sd = &kernel_split_state.sd[ray_index];
				*bssrdf_sd = *sd; /* note: copy happens each iteration of inner loop, this is
				                   * important as the indirect path will write into bssrdf_sd */

				SubsurfaceIntersection ss_isect_private = *ss_isect;
				subsurface_scatter_multi_setup(kg,
				                               &ss_isect_private,
				                               hit,
				                               bssrdf_sd,
				                               &branched_state->path_state,
				                               branched_state->path_state.flag,
				                               sc,
				                               true);
				*ss_isect = ss_isect_private;

				ccl_global PathState *hit_state = &kernel_split_state.path_state[ray_index];
				*hit_state = branched_state->path_state;

				path_state_branch(hit_state, j, num_samples);

#ifdef __VOLUME__
				if(need_update_volume_stack) {
					/* Setup ray from previous surface point to the new one. */
					float3 P = ray_offset(bssrdf_sd->P, -bssrdf_sd->Ng);
					volume_ray.D = normalize_len(P - volume_ray.P, &volume_ray.t);

					/* this next part is expensive as it does scene intersection so only do once */
					if(branched_state->next_closure == 0 && branched_state->next_sample == 0) {
						for(int k = 0; k < VOLUME_STACK_SIZE; k++) {
							branched_state->volume_stack[k] = hit_state->volume_stack[k];
						}

						kernel_volume_stack_update_for_subsurface(kg,
						                                          emission_sd,
						                                          &volume_ray,
						                                          branched_state->volume_stack);
					}

					for(int k = 0; k < VOLUME_STACK_SIZE; k++) {
						hit_state->volume_stack[k] = branched_state->volume_stack[k];
					}
				}
#endif  /* __VOLUME__ */

#ifdef __EMISSION__
				if(branched_state->next_closure == 0 && branched_state->next_sample == 0) {
					/* direct light */
					if(kernel_data.integrator.use_direct_light) {
						int all = (kernel_data.integrator.sample_all_lights_direct) ||
							      (branched_state->path_state.flag & PATH_RAY_SHADOW_CATCHER);
						kernel_branched_path_surface_connect_light(kg,
						                                           &rng,
						                                           bssrdf_sd,
						                                           emission_sd,
						                                           hit_state,
						                                           branched_state->throughput,
						                                           num_samples_inv,
						                                           L,
						                                           all);
					}
				}
#endif  /* __EMISSION__ */

				/* indirect light */
				if(kernel_split_branched_path_surface_indirect_light_iter(kg,
				                                                          ray_index,
				                                                          num_samples_inv,
				                                                          bssrdf_sd,
				                                                          false,
				                                                          false))
				{
					branched_state->ss_next_closure = i;
					branched_state->ss_next_sample = j;
					branched_state->next_hit = hit;

					return true;
				}

				branched_state->next_closure = 0;
			}

			branched_state->next_hit = 0;
		}

		branched_state->ss_next_sample = 0;
	}

	branched_state->ss_next_closure = sd->num_closure;

	branched_state->waiting_on_shared_samples = (branched_state->shared_sample_count > 0);
	if(branched_state->waiting_on_shared_samples) {
		return true;
	}

	kernel_split_branched_path_indirect_loop_end(kg, ray_index);

	return false;
}

#endif  /* __BRANCHED_PATH__ && __SUBSURFACE__ */

ccl_device void kernel_subsurface_scatter(KernelGlobals *kg)
{
	int thread_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	if(thread_index == 0) {
		/* We will empty both queues in this kernel. */
		kernel_split_params.queue_index[QUEUE_ACTIVE_AND_REGENERATED_RAYS] = 0;
		kernel_split_params.queue_index[QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS] = 0;
	}

	int ray_index = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	ray_index = get_ray_index(kg, ray_index,
	                          QUEUE_ACTIVE_AND_REGENERATED_RAYS,
	                          kernel_split_state.queue_data,
	                          kernel_split_params.queue_size,
	                          1);
	get_ray_index(kg, thread_index,
	              QUEUE_HITBG_BUFF_UPDATE_TOREGEN_RAYS,
	              kernel_split_state.queue_data,
	              kernel_split_params.queue_size,
	              1);

#ifdef __SUBSURFACE__
	ccl_global char *ray_state = kernel_split_state.ray_state;

	if(IS_STATE(ray_state, ray_index, RAY_ACTIVE)) {
		ccl_global PathState *state = &kernel_split_state.path_state[ray_index];
		PathRadiance *L = &kernel_split_state.path_radiance[ray_index];
		RNG rng = kernel_split_state.rng[ray_index];
		ccl_global Ray *ray = &kernel_split_state.ray[ray_index];
		ccl_global float3 *throughput = &kernel_split_state.throughput[ray_index];
		ccl_global SubsurfaceIndirectRays *ss_indirect = &kernel_split_state.ss_rays[ray_index];
		ShaderData *sd = &kernel_split_state.sd[ray_index];
		ShaderData *emission_sd = &kernel_split_state.sd_DL_shadow[ray_index];

		if(sd->flag & SD_BSSRDF) {

#ifdef __BRANCHED_PATH__
			if(!kernel_data.integrator.branched) {
#endif
				if(kernel_path_subsurface_scatter(kg,
				                                  sd,
				                                  emission_sd,
				                                  L,
				                                  state,
				                                  &rng,
				                                  ray,
				                                  throughput,
				                                  ss_indirect))
				{
					kernel_split_path_end(kg, ray_index);
				}
#ifdef __BRANCHED_PATH__
			}
			else if(IS_FLAG(ray_state, ray_index, RAY_BRANCHED_INDIRECT)) {
				float bssrdf_probability;
				ShaderClosure *sc = subsurface_scatter_pick_closure(kg, sd, &bssrdf_probability);

				/* modify throughput for picking bssrdf or bsdf */
				*throughput *= bssrdf_probability;

				/* do bssrdf scatter step if we picked a bssrdf closure */
				if(sc) {
					uint lcg_state = lcg_state_init(&rng, state->rng_offset, state->sample, 0x68bc21eb);
					float bssrdf_u, bssrdf_v;
					path_state_rng_2D(kg,
					                  &rng,
					                  state,
					                  PRNG_BSDF_U,
					                  &bssrdf_u, &bssrdf_v);
					subsurface_scatter_step(kg,
					                        sd,
					                        state,
					                        state->flag,
					                        sc,
					                        &lcg_state,
					                        bssrdf_u, bssrdf_v,
					                        false);
				}
			}
			else {
				kernel_split_branched_path_subsurface_indirect_light_init(kg, ray_index);

				if(kernel_split_branched_path_subsurface_indirect_light_iter(kg, ray_index)) {
					ASSIGN_RAY_STATE(ray_state, ray_index, RAY_REGENERATED);
				}
			}
#endif
		}
		kernel_split_state.rng[ray_index] = rng;
	}

#  ifdef __BRANCHED_PATH__
	if(ccl_global_id(0) == 0 && ccl_global_id(1) == 0) {
		kernel_split_params.queue_index[QUEUE_SUBSURFACE_INDIRECT_ITER] = 0;
	}

	/* iter loop */
	ray_index = get_ray_index(kg, ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0),
	                          QUEUE_SUBSURFACE_INDIRECT_ITER,
	                          kernel_split_state.queue_data,
	                          kernel_split_params.queue_size,
	                          1);

	if(IS_STATE(ray_state, ray_index, RAY_SUBSURFACE_INDIRECT_NEXT_ITER)) {
		/* for render passes, sum and reset indirect light pass variables
		 * for the next samples */
		path_radiance_sum_indirect(&kernel_split_state.path_radiance[ray_index]);
		path_radiance_reset_indirect(&kernel_split_state.path_radiance[ray_index]);

		if(kernel_split_branched_path_subsurface_indirect_light_iter(kg, ray_index)) {
			ASSIGN_RAY_STATE(ray_state, ray_index, RAY_REGENERATED);
		}
	}
#  endif  /* __BRANCHED_PATH__ */

#endif  /* __SUBSURFACE__ */

}

CCL_NAMESPACE_END
