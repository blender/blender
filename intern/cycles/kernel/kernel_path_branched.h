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

#ifdef __BRANCHED_PATH__

ccl_device_inline void kernel_branched_path_ao(KernelGlobals *kg,
                                               ShaderData *sd,
                                               ShaderData *emission_sd,
                                               PathRadiance *L,
                                               PathState *state,
                                               RNG *rng,
                                               float3 throughput)
{
	int num_samples = kernel_data.integrator.ao_samples;
	float num_samples_inv = 1.0f/num_samples;
	float ao_factor = kernel_data.background.ao_factor;
	float3 ao_N;
	float3 ao_bsdf = shader_bsdf_ao(kg, sd, ao_factor, &ao_N);
	float3 ao_alpha = shader_bsdf_alpha(kg, sd);

	for(int j = 0; j < num_samples; j++) {
		float bsdf_u, bsdf_v;
		path_branched_rng_2D(kg, rng, state, j, num_samples, PRNG_BSDF_U, &bsdf_u, &bsdf_v);

		float3 ao_D;
		float ao_pdf;

		sample_cos_hemisphere(ao_N, bsdf_u, bsdf_v, &ao_D, &ao_pdf);

		if(dot(ccl_fetch(sd, Ng), ao_D) > 0.0f && ao_pdf != 0.0f) {
			Ray light_ray;
			float3 ao_shadow;

			light_ray.P = ray_offset(ccl_fetch(sd, P), ccl_fetch(sd, Ng));
			light_ray.D = ao_D;
			light_ray.t = kernel_data.background.ao_distance;
#ifdef __OBJECT_MOTION__
			light_ray.time = ccl_fetch(sd, time);
#endif  /* __OBJECT_MOTION__ */
			light_ray.dP = ccl_fetch(sd, dP);
			light_ray.dD = differential3_zero();

			if(!shadow_blocked(kg, emission_sd, state, &light_ray, &ao_shadow))
				path_radiance_accum_ao(L, throughput*num_samples_inv, ao_alpha, ao_bsdf, ao_shadow, state->bounce);
		}
	}
}


/* bounce off surface and integrate indirect light */
ccl_device_noinline void kernel_branched_path_surface_indirect_light(KernelGlobals *kg,
	RNG *rng, ShaderData *sd, ShaderData *indirect_sd, ShaderData *emission_sd,
	float3 throughput, float num_samples_adjust, PathState *state, PathRadiance *L)
{
	for(int i = 0; i < ccl_fetch(sd, num_closure); i++) {
		const ShaderClosure *sc = &ccl_fetch(sd, closure)[i];

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
		RNG bsdf_rng = cmj_hash(*rng, i);

		for(int j = 0; j < num_samples; j++) {
			PathState ps = *state;
			float3 tp = throughput;
			Ray bsdf_ray;

			if(!kernel_branched_path_surface_bounce(kg,
			                                        &bsdf_rng,
			                                        sd,
			                                        sc,
			                                        j,
			                                        num_samples,
			                                        &tp,
			                                        &ps,
			                                        L,
			                                        &bsdf_ray))
			{
				continue;
			}

			kernel_path_indirect(kg,
			                     indirect_sd,
			                     emission_sd,
			                     rng,
			                     &bsdf_ray,
			                     tp*num_samples_inv,
			                     num_samples,
			                     &ps,
			                     L);

			/* for render passes, sum and reset indirect light pass variables
			 * for the next samples */
			path_radiance_sum_indirect(L);
			path_radiance_reset_indirect(L);
		}
	}
}

#ifdef __SUBSURFACE__
ccl_device void kernel_branched_path_subsurface_scatter(KernelGlobals *kg,
                                                        ShaderData *sd,
                                                        ShaderData *indirect_sd,
                                                        ShaderData *emission_sd,
                                                        PathRadiance *L,
                                                        PathState *state,
                                                        RNG *rng,
                                                        Ray *ray,
                                                        float3 throughput)
{
	for(int i = 0; i < ccl_fetch(sd, num_closure); i++) {
		ShaderClosure *sc = &ccl_fetch(sd, closure)[i];

		if(!CLOSURE_IS_BSSRDF(sc->type))
			continue;

		/* set up random number generator */
		uint lcg_state = lcg_state_init(rng, state, 0x68bc21eb);
		int num_samples = kernel_data.integrator.subsurface_samples;
		float num_samples_inv = 1.0f/num_samples;
		RNG bssrdf_rng = cmj_hash(*rng, i);

		/* do subsurface scatter step with copy of shader data, this will
		 * replace the BSSRDF with a diffuse BSDF closure */
		for(int j = 0; j < num_samples; j++) {
			SubsurfaceIntersection ss_isect;
			float bssrdf_u, bssrdf_v;
			path_branched_rng_2D(kg, &bssrdf_rng, state, j, num_samples, PRNG_BSDF_U, &bssrdf_u, &bssrdf_v);
			int num_hits = subsurface_scatter_multi_intersect(kg,
			                                                  &ss_isect,
			                                                  sd,
			                                                  sc,
			                                                  &lcg_state,
			                                                  bssrdf_u, bssrdf_v,
			                                                  true);
#ifdef __VOLUME__
			Ray volume_ray = *ray;
			bool need_update_volume_stack = kernel_data.integrator.use_volumes &&
			                                ccl_fetch(sd, flag) & SD_OBJECT_INTERSECTS_VOLUME;
#endif  /* __VOLUME__ */

			/* compute lighting with the BSDF closure */
			for(int hit = 0; hit < num_hits; hit++) {
				ShaderData bssrdf_sd = *sd;
				subsurface_scatter_multi_setup(kg,
				                               &ss_isect,
				                               hit,
				                               &bssrdf_sd,
				                               state,
				                               state->flag,
				                               sc,
				                               true);

				PathState hit_state = *state;

				path_state_branch(&hit_state, j, num_samples);

#ifdef __VOLUME__
				if(need_update_volume_stack) {
					/* Setup ray from previous surface point to the new one. */
					float3 P = ray_offset(bssrdf_sd.P, -bssrdf_sd.Ng);
					volume_ray.D = normalize_len(P - volume_ray.P,
					                             &volume_ray.t);

					kernel_volume_stack_update_for_subsurface(
					    kg,
					    emission_sd,
					    &volume_ray,
					    hit_state.volume_stack);
				}
#endif  /* __VOLUME__ */

#ifdef __EMISSION__
				/* direct light */
				if(kernel_data.integrator.use_direct_light) {
					int all = kernel_data.integrator.sample_all_lights_direct;
					kernel_branched_path_surface_connect_light(
					        kg,
					        rng,
					        &bssrdf_sd,
					        emission_sd,
					        &hit_state,
					        throughput,
					        num_samples_inv,
					        L,
					        all);
				}
#endif  /* __EMISSION__ */

				/* indirect light */
				kernel_branched_path_surface_indirect_light(
				        kg,
				        rng,
				        &bssrdf_sd,
				        indirect_sd,
				        emission_sd,
				        throughput,
				        num_samples_inv,
				        &hit_state,
				        L);
			}
		}
	}
}
#endif  /* __SUBSURFACE__ */

ccl_device float4 kernel_branched_path_integrate(KernelGlobals *kg, RNG *rng, int sample, Ray ray, ccl_global float *buffer)
{
	/* initialize */
	PathRadiance L;
	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
	float L_transparent = 0.0f;

	path_radiance_init(&L, kernel_data.film.use_light_pass);

	/* shader data memory used for both volumes and surfaces, saves stack space */
	ShaderData sd;
	/* shader data used by emission, shadows, volume stacks, indirect path */
	ShaderData emission_sd, indirect_sd;

	PathState state;
	path_state_init(kg, &emission_sd, &state, rng, sample, &ray);

#ifdef __KERNEL_DEBUG__
	DebugData debug_data;
	debug_data_init(&debug_data);
#endif  /* __KERNEL_DEBUG__ */

	/* Main Loop
	 * Here we only handle transparency intersections from the camera ray.
	 * Indirect bounces are handled in kernel_branched_path_surface_indirect_light().
	 */
	for(;;) {
		/* intersect scene */
		Intersection isect;
		uint visibility = path_state_ray_visibility(kg, &state);

#ifdef __HAIR__
		float difl = 0.0f, extmax = 0.0f;
		uint lcg_state = 0;

		if(kernel_data.bvh.have_curves) {
			if(kernel_data.cam.resolution == 1) {
				float3 pixdiff = ray.dD.dx + ray.dD.dy;
				/*pixdiff = pixdiff - dot(pixdiff, ray.D)*ray.D;*/
				difl = kernel_data.curve.minimum_width * len(pixdiff) * 0.5f;
			}

			extmax = kernel_data.curve.maximum_width;
			lcg_state = lcg_state_init(rng, &state, 0x51633e2d);
		}

		bool hit = scene_intersect(kg, ray, visibility, &isect, &lcg_state, difl, extmax);
#else
		bool hit = scene_intersect(kg, ray, visibility, &isect, NULL, 0.0f, 0.0f);
#endif  /* __HAIR__ */

#ifdef __KERNEL_DEBUG__
		debug_data.num_bvh_traversed_nodes += isect.num_traversed_nodes;
		debug_data.num_bvh_traversed_instances += isect.num_traversed_instances;
		debug_data.num_bvh_intersections += isect.num_intersections;
		debug_data.num_ray_bounces++;
#endif  /* __KERNEL_DEBUG__ */

#ifdef __VOLUME__
		/* Sanitize volume stack. */
		if(!hit) {
			kernel_volume_clean_stack(kg, state.volume_stack);
		}
		/* volume attenuation, emission, scatter */
		if(state.volume_stack[0].shader != SHADER_NONE) {
			Ray volume_ray = ray;
			volume_ray.t = (hit)? isect.t: FLT_MAX;
			
			bool heterogeneous = volume_stack_is_heterogeneous(kg, state.volume_stack);

#ifdef __VOLUME_DECOUPLED__
			/* decoupled ray marching only supported on CPU */

			/* cache steps along volume for repeated sampling */
			VolumeSegment volume_segment;

			shader_setup_from_volume(kg, &sd, &volume_ray);
			kernel_volume_decoupled_record(kg, &state,
				&volume_ray, &sd, &volume_segment, heterogeneous);

			/* direct light sampling */
			if(volume_segment.closure_flag & SD_SCATTER) {
				volume_segment.sampling_method = volume_stack_sampling_method(kg, state.volume_stack);

				int all = kernel_data.integrator.sample_all_lights_direct;

				kernel_branched_path_volume_connect_light(kg, rng, &sd,
					&emission_sd, throughput, &state, &L, all,
					&volume_ray, &volume_segment);

				/* indirect light sampling */
				int num_samples = kernel_data.integrator.volume_samples;
				float num_samples_inv = 1.0f/num_samples;

				for(int j = 0; j < num_samples; j++) {
					/* workaround to fix correlation bug in T38710, can find better solution
					 * in random number generator later, for now this is done here to not impact
					 * performance of rendering without volumes */
					RNG tmp_rng = cmj_hash(*rng, state.rng_offset);

					PathState ps = state;
					Ray pray = ray;
					float3 tp = throughput;

					/* branch RNG state */
					path_state_branch(&ps, j, num_samples);

					/* scatter sample. if we use distance sampling and take just one
					 * sample for direct and indirect light, we could share this
					 * computation, but makes code a bit complex */
					float rphase = path_state_rng_1D_for_decision(kg, &tmp_rng, &ps, PRNG_PHASE);
					float rscatter = path_state_rng_1D_for_decision(kg, &tmp_rng, &ps, PRNG_SCATTER_DISTANCE);

					VolumeIntegrateResult result = kernel_volume_decoupled_scatter(kg,
						&ps, &pray, &sd, &tp, rphase, rscatter, &volume_segment, NULL, false);

					(void)result;
					kernel_assert(result == VOLUME_PATH_SCATTERED);

					if(kernel_path_volume_bounce(kg,
					                             rng,
					                             &sd,
					                             &tp,
					                             &ps,
					                             &L,
					                             &pray))
					{
						kernel_path_indirect(kg,
						                     &indirect_sd,
						                     &emission_sd,
						                     rng,
						                     &pray,
						                     tp*num_samples_inv,
						                     num_samples,
						                     &ps,
						                     &L);

						/* for render passes, sum and reset indirect light pass variables
						 * for the next samples */
						path_radiance_sum_indirect(&L);
						path_radiance_reset_indirect(&L);
					}
				}
			}

			/* emission and transmittance */
			if(volume_segment.closure_flag & SD_EMISSION)
				path_radiance_accum_emission(&L, throughput, volume_segment.accum_emission, state.bounce);
			throughput *= volume_segment.accum_transmittance;

			/* free cached steps */
			kernel_volume_decoupled_free(kg, &volume_segment);
#else
			/* GPU: no decoupled ray marching, scatter probalistically */
			int num_samples = kernel_data.integrator.volume_samples;
			float num_samples_inv = 1.0f/num_samples;

			/* todo: we should cache the shader evaluations from stepping
			 * through the volume, for now we redo them multiple times */

			for(int j = 0; j < num_samples; j++) {
				PathState ps = state;
				Ray pray = ray;
				float3 tp = throughput * num_samples_inv;

				/* branch RNG state */
				path_state_branch(&ps, j, num_samples);

				VolumeIntegrateResult result = kernel_volume_integrate(
					kg, &ps, &sd, &volume_ray, &L, &tp, rng, heterogeneous);

#ifdef __VOLUME_SCATTER__
				if(result == VOLUME_PATH_SCATTERED) {
					/* todo: support equiangular, MIS and all light sampling.
					 * alternatively get decoupled ray marching working on the GPU */
					kernel_path_volume_connect_light(kg, rng, &sd, &emission_sd, tp, &state, &L);

					if(kernel_path_volume_bounce(kg,
					                             rng,
					                             &sd,
					                             &tp,
					                             &ps,
					                             &L,
					                             &pray))
					{
						kernel_path_indirect(kg,
						                     &indirect_sd,
						                     &emission_sd,
						                     rng,
						                     &pray,
						                     tp,
						                     num_samples,
						                     &ps,
						                     &L);

						/* for render passes, sum and reset indirect light pass variables
						 * for the next samples */
						path_radiance_sum_indirect(&L);
						path_radiance_reset_indirect(&L);
					}
				}
#endif  /* __VOLUME_SCATTER__ */
			}

			/* todo: avoid this calculation using decoupled ray marching */
			kernel_volume_shadow(kg, &emission_sd, &state, &volume_ray, &throughput);
#endif  /* __VOLUME_DECOUPLED__ */
		}
#endif  /* __VOLUME__ */

		if(!hit) {
			/* eval background shader if nothing hit */
			if(kernel_data.background.transparent) {
				L_transparent += average(throughput);

#ifdef __PASSES__
				if(!(kernel_data.film.pass_flag & PASS_BACKGROUND))
#endif  /* __PASSES__ */
					break;
			}

#ifdef __BACKGROUND__
			/* sample background shader */
			float3 L_background = indirect_background(kg, &emission_sd, &state, &ray);
			path_radiance_accum_background(&L, throughput, L_background, state.bounce);
#endif  /* __BACKGROUND__ */

			break;
		}

		/* setup shading */
		shader_setup_from_ray(kg, &sd, &isect, &ray);
		shader_eval_surface(kg, &sd, rng, &state, 0.0f, state.flag, SHADER_CONTEXT_MAIN);
		shader_merge_closures(&sd);

		/* holdout */
#ifdef __HOLDOUT__
		if(sd.flag & (SD_HOLDOUT|SD_HOLDOUT_MASK)) {
			if(kernel_data.background.transparent) {
				float3 holdout_weight;
				
				if(sd.flag & SD_HOLDOUT_MASK)
					holdout_weight = make_float3(1.0f, 1.0f, 1.0f);
				else
					holdout_weight = shader_holdout_eval(kg, &sd);

				/* any throughput is ok, should all be identical here */
				L_transparent += average(holdout_weight*throughput);
			}

			if(sd.flag & SD_HOLDOUT_MASK)
				break;
		}
#endif  /* __HOLDOUT__ */

		/* holdout mask objects do not write data passes */
		kernel_write_data_passes(kg, buffer, &L, &sd, sample, &state, throughput);

#ifdef __EMISSION__
		/* emission */
		if(sd.flag & SD_EMISSION) {
			float3 emission = indirect_primitive_emission(kg, &sd, isect.t, state.flag, state.ray_pdf);
			path_radiance_accum_emission(&L, throughput, emission, state.bounce);
		}
#endif  /* __EMISSION__ */

		/* transparency termination */
		if(state.flag & PATH_RAY_TRANSPARENT) {
			/* path termination. this is a strange place to put the termination, it's
			 * mainly due to the mixed in MIS that we use. gives too many unneeded
			 * shader evaluations, only need emission if we are going to terminate */
			float probability = path_state_terminate_probability(kg, &state, throughput);

			if(probability == 0.0f) {
				break;
			}
			else if(probability != 1.0f) {
				float terminate = path_state_rng_1D_for_decision(kg, rng, &state, PRNG_TERMINATE);

				if(terminate >= probability)
					break;

				throughput /= probability;
			}
		}

#ifdef __AO__
		/* ambient occlusion */
		if(kernel_data.integrator.use_ambient_occlusion || (sd.flag & SD_AO)) {
			kernel_branched_path_ao(kg, &sd, &emission_sd, &L, &state, rng, throughput);
		}
#endif  /* __AO__ */

#ifdef __SUBSURFACE__
		/* bssrdf scatter to a different location on the same object */
		if(sd.flag & SD_BSSRDF) {
			kernel_branched_path_subsurface_scatter(kg, &sd, &indirect_sd, &emission_sd,
			                                        &L, &state, rng, &ray, throughput);
		}
#endif  /* __SUBSURFACE__ */

		if(!(sd.flag & SD_HAS_ONLY_VOLUME)) {
			PathState hit_state = state;

#ifdef __EMISSION__
			/* direct light */
			if(kernel_data.integrator.use_direct_light) {
				int all = kernel_data.integrator.sample_all_lights_direct;
				kernel_branched_path_surface_connect_light(kg, rng,
					&sd, &emission_sd, &hit_state, throughput, 1.0f, &L, all);
			}
#endif  /* __EMISSION__ */

			/* indirect light */
			kernel_branched_path_surface_indirect_light(kg, rng,
				&sd, &indirect_sd, &emission_sd, throughput, 1.0f, &hit_state, &L);

			/* continue in case of transparency */
			throughput *= shader_bsdf_transparency(kg, &sd);

			if(is_zero(throughput))
				break;
		}

		/* Update Path State */
		state.flag |= PATH_RAY_TRANSPARENT;
		state.transparent_bounce++;

		ray.P = ray_offset(sd.P, -sd.Ng);
		ray.t -= sd.ray_length; /* clipping works through transparent */


#ifdef __RAY_DIFFERENTIALS__
		ray.dP = sd.dP;
		ray.dD.dx = -sd.dI.dx;
		ray.dD.dy = -sd.dI.dy;
#endif  /* __RAY_DIFFERENTIALS__ */

#ifdef __VOLUME__
		/* enter/exit volume */
		kernel_volume_stack_enter_exit(kg, &sd, state.volume_stack);
#endif  /* __VOLUME__ */
	}

	float3 L_sum = path_radiance_clamp_and_sum(kg, &L);

	kernel_write_light_passes(kg, buffer, &L, sample);

#ifdef __KERNEL_DEBUG__
	kernel_write_debug_passes(kg, buffer, &state, &debug_data, sample);
#endif  /* __KERNEL_DEBUG__ */

	return make_float4(L_sum.x, L_sum.y, L_sum.z, 1.0f - L_transparent);
}

ccl_device void kernel_branched_path_trace(KernelGlobals *kg,
	ccl_global float *buffer, ccl_global uint *rng_state,
	int sample, int x, int y, int offset, int stride)
{
	/* buffer offset */
	int index = offset + x + y*stride;
	int pass_stride = kernel_data.film.pass_stride;

	rng_state += index;
	buffer += index*pass_stride;

	/* initialize random numbers and ray */
	RNG rng;
	Ray ray;

	kernel_path_trace_setup(kg, rng_state, sample, x, y, &rng, &ray);

	/* integrate */
	float4 L;

	if(ray.t != 0.0f)
		L = kernel_branched_path_integrate(kg, &rng, sample, ray, buffer);
	else
		L = make_float4(0.0f, 0.0f, 0.0f, 0.0f);

	/* accumulate result in output buffer */
	kernel_write_pass_float4(buffer, sample, L);

	path_rng_end(kg, rng_state, rng);
}

#endif  /* __BRANCHED_PATH__ */

CCL_NAMESPACE_END

