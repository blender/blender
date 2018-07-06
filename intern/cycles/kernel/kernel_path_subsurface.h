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
        ccl_addr_space Ray *ray,
        ccl_addr_space float3 *throughput,
        ccl_addr_space SubsurfaceIndirectRays *ss_indirect)
{
	float bssrdf_u, bssrdf_v;
	path_state_rng_2D(kg, state, PRNG_BSDF_U, &bssrdf_u, &bssrdf_v);

	const ShaderClosure *sc = shader_bssrdf_pick(sd, throughput, &bssrdf_u);

	/* do bssrdf scatter step if we picked a bssrdf closure */
	if(sc) {
		/* We should never have two consecutive BSSRDF bounces,
		 * the second one should be converted to a diffuse BSDF to
		 * avoid this.
		 */
		kernel_assert(!(state->flag & PATH_RAY_DIFFUSE_ANCESTOR));

		uint lcg_state = lcg_state_init_addrspace(state, 0x68bc21eb);

		LocalIntersection ss_isect;
		int num_hits = subsurface_scatter_multi_intersect(kg,
		                                                  &ss_isect,
		                                                  sd,
		                                                  state,
		                                                  sc,
		                                                  &lcg_state,
		                                                  bssrdf_u, bssrdf_v,
		                                                  false);
#  ifdef __VOLUME__
		bool need_update_volume_stack =
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
			                               sc);

			kernel_path_surface_connect_light(kg, sd, emission_sd, *throughput, state, L);

			ccl_addr_space PathState *hit_state = &ss_indirect->state[ss_indirect->num_rays];
			ccl_addr_space Ray *hit_ray = &ss_indirect->rays[ss_indirect->num_rays];
			ccl_addr_space float3 *hit_tp = &ss_indirect->throughputs[ss_indirect->num_rays];
			PathRadianceState *hit_L_state = &ss_indirect->L_state[ss_indirect->num_rays];

			*hit_state = *state;
			*hit_ray = *ray;
			*hit_tp = *throughput;
			*hit_L_state = L->state;

			hit_state->rng_offset += PRNG_BOUNCE_NUM;

			if(kernel_path_surface_bounce(kg,
			                              sd,
			                              hit_tp,
			                              hit_state,
			                              hit_L_state,
			                              hit_ray))
			{
#  ifdef __LAMP_MIS__
				hit_state->ray_t = 0.0f;
#  endif  /* __LAMP_MIS__ */

#  ifdef __VOLUME__
				if(need_update_volume_stack) {
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
				ss_indirect->num_rays++;
			}
		}
		return true;
	}
	return false;
}

ccl_device_inline void kernel_path_subsurface_init_indirect(
        ccl_addr_space SubsurfaceIndirectRays *ss_indirect)
{
	ss_indirect->num_rays = 0;
}

ccl_device void kernel_path_subsurface_setup_indirect(
        KernelGlobals *kg,
        ccl_addr_space SubsurfaceIndirectRays *ss_indirect,
        ccl_addr_space PathState *state,
        ccl_addr_space Ray *ray,
        PathRadiance *L,
        ccl_addr_space float3 *throughput)
{
	/* Setup state, ray and throughput for indirect SSS rays. */
	ss_indirect->num_rays--;

	path_radiance_sum_indirect(L);
	path_radiance_reset_indirect(L);

	*state = ss_indirect->state[ss_indirect->num_rays];
	*ray = ss_indirect->rays[ss_indirect->num_rays];
	L->state = ss_indirect->L_state[ss_indirect->num_rays];
	*throughput = ss_indirect->throughputs[ss_indirect->num_rays];

	state->rng_offset += ss_indirect->num_rays * PRNG_BOUNCE_NUM;
}

#endif  /* __SUBSURFACE__ */

CCL_NAMESPACE_END
