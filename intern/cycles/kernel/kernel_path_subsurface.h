/*
 * Copyright 2017 Blender Foundation
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

#ifdef __SUBSURFACE__
#  ifndef __KERNEL_CUDA__
ccl_device
#  else
ccl_device_inline
#  endif
bool kernel_path_subsurface_scatter(
        KernelGlobals *kg,
        ShaderData *sd,
        ShaderData *emission_sd,
        PathRadiance *L,
        ccl_addr_space PathState *state,
        RNG *rng,
        ccl_addr_space Ray *ray,
        ccl_addr_space float3 *throughput,
        ccl_addr_space SubsurfaceIndirectRays *ss_indirect)
{
	float bssrdf_probability;
	ShaderClosure *sc = subsurface_scatter_pick_closure(kg, sd, &bssrdf_probability);

	/* modify throughput for picking bssrdf or bsdf */
	*throughput *= bssrdf_probability;

	/* do bssrdf scatter step if we picked a bssrdf closure */
	if(sc) {
		/* We should never have two consecutive BSSRDF bounces,
		 * the second one should be converted to a diffuse BSDF to
		 * avoid this.
		 */
		kernel_assert(!ss_indirect->tracing);

		uint lcg_state = lcg_state_init(rng, state->rng_offset, state->sample, 0x68bc21eb);

		SubsurfaceIntersection ss_isect;
		float bssrdf_u, bssrdf_v;
		path_state_rng_2D(kg, rng, state, PRNG_BSDF_U, &bssrdf_u, &bssrdf_v);
		int num_hits = subsurface_scatter_multi_intersect(kg,
		                                                  &ss_isect,
		                                                  sd,
		                                                  sc,
		                                                  &lcg_state,
		                                                  bssrdf_u, bssrdf_v,
		                                                  false);
#  ifdef __VOLUME__
		ss_indirect->need_update_volume_stack =
		        kernel_data.integrator.use_volumes &&
		        sd->object_flag & SD_OBJECT_INTERSECTS_VOLUME;
#  endif  /* __VOLUME__ */

		/* compute lighting with the BSDF closure */
		for(int hit = 0; hit < num_hits; hit++) {
			/* NOTE: We reuse the existing ShaderData, we assume the path
			 * integration loop stops when this function returns true.
			 */
			subsurface_scatter_multi_setup(kg,
			                               &ss_isect,
			                               hit,
			                               sd,
			                               state,
			                               state->flag,
			                               sc,
			                               false);

			ccl_addr_space PathState *hit_state = &ss_indirect->state[ss_indirect->num_rays];
			ccl_addr_space Ray *hit_ray = &ss_indirect->rays[ss_indirect->num_rays];
			ccl_addr_space float3 *hit_tp = &ss_indirect->throughputs[ss_indirect->num_rays];
			PathRadiance *hit_L = &ss_indirect->L[ss_indirect->num_rays];

			*hit_state = *state;
			*hit_ray = *ray;
			*hit_tp = *throughput;

			hit_state->rng_offset += PRNG_BOUNCE_NUM;

			path_radiance_init(hit_L, kernel_data.film.use_light_pass);
			hit_L->direct_throughput = L->direct_throughput;
			path_radiance_copy_indirect(hit_L, L);

			kernel_path_surface_connect_light(kg, rng, sd, emission_sd, *hit_tp, state, hit_L);

			if(kernel_path_surface_bounce(kg,
			                              rng,
			                              sd,
			                              hit_tp,
			                              hit_state,
			                              hit_L,
			                              hit_ray))
			{
#  ifdef __LAMP_MIS__
				hit_state->ray_t = 0.0f;
#  endif  /* __LAMP_MIS__ */

#  ifdef __VOLUME__
				if(ss_indirect->need_update_volume_stack) {
					Ray volume_ray = *ray;
					/* Setup ray from previous surface point to the new one. */
					volume_ray.D = normalize_len(hit_ray->P - volume_ray.P,
					                             &volume_ray.t);

					kernel_volume_stack_update_for_subsurface(
					    kg,
					    emission_sd,
					    &volume_ray,
					    hit_state->volume_stack);
				}
#  endif  /* __VOLUME__ */
				path_radiance_reset_indirect(L);
				ss_indirect->num_rays++;
			}
			else {
				path_radiance_accum_sample(L, hit_L, 1);
			}
		}
		return true;
	}
	return false;
}

ccl_device_inline void kernel_path_subsurface_init_indirect(
        ccl_addr_space SubsurfaceIndirectRays *ss_indirect)
{
	ss_indirect->tracing = false;
	ss_indirect->num_rays = 0;
}

ccl_device void kernel_path_subsurface_accum_indirect(
        ccl_addr_space SubsurfaceIndirectRays *ss_indirect,
        PathRadiance *L)
{
	if(ss_indirect->tracing) {
		path_radiance_sum_indirect(L);
		path_radiance_accum_sample(&ss_indirect->direct_L, L, 1);
		if(ss_indirect->num_rays == 0) {
			*L = ss_indirect->direct_L;
		}
	}
}

ccl_device void kernel_path_subsurface_setup_indirect(
        KernelGlobals *kg,
        ccl_addr_space SubsurfaceIndirectRays *ss_indirect,
        ccl_addr_space PathState *state,
        ccl_addr_space Ray *ray,
        PathRadiance *L,
        ccl_addr_space float3 *throughput)
{
	if(!ss_indirect->tracing) {
		ss_indirect->direct_L = *L;
	}
	ss_indirect->tracing = true;

	/* Setup state, ray and throughput for indirect SSS rays. */
	ss_indirect->num_rays--;

	ccl_addr_space Ray *indirect_ray = &ss_indirect->rays[ss_indirect->num_rays];
	PathRadiance *indirect_L = &ss_indirect->L[ss_indirect->num_rays];

	*state = ss_indirect->state[ss_indirect->num_rays];
	*ray = *indirect_ray;
	*L = *indirect_L;
	*throughput = ss_indirect->throughputs[ss_indirect->num_rays];

	state->rng_offset += ss_indirect->num_rays * PRNG_BOUNCE_NUM;
}

#endif  /* __SUBSURFACE__ */

CCL_NAMESPACE_END

