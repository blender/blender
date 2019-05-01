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
                                               ccl_addr_space PathState *state,
                                               float3 throughput)
{
  int num_samples = kernel_data.integrator.ao_samples;
  float num_samples_inv = 1.0f / num_samples;
  float ao_factor = kernel_data.background.ao_factor;
  float3 ao_N;
  float3 ao_bsdf = shader_bsdf_ao(kg, sd, ao_factor, &ao_N);
  float3 ao_alpha = shader_bsdf_alpha(kg, sd);

  for (int j = 0; j < num_samples; j++) {
    float bsdf_u, bsdf_v;
    path_branched_rng_2D(
        kg, state->rng_hash, state, j, num_samples, PRNG_BSDF_U, &bsdf_u, &bsdf_v);

    float3 ao_D;
    float ao_pdf;

    sample_cos_hemisphere(ao_N, bsdf_u, bsdf_v, &ao_D, &ao_pdf);

    if (dot(sd->Ng, ao_D) > 0.0f && ao_pdf != 0.0f) {
      Ray light_ray;
      float3 ao_shadow;

      light_ray.P = ray_offset(sd->P, sd->Ng);
      light_ray.D = ao_D;
      light_ray.t = kernel_data.background.ao_distance;
      light_ray.time = sd->time;
      light_ray.dP = sd->dP;
      light_ray.dD = differential3_zero();

      if (!shadow_blocked(kg, sd, emission_sd, state, &light_ray, &ao_shadow)) {
        path_radiance_accum_ao(
            L, state, throughput * num_samples_inv, ao_alpha, ao_bsdf, ao_shadow);
      }
      else {
        path_radiance_accum_total_ao(L, state, throughput * num_samples_inv, ao_bsdf);
      }
    }
  }
}

#  ifndef __SPLIT_KERNEL__

#    ifdef __VOLUME__
ccl_device_forceinline void kernel_branched_path_volume(KernelGlobals *kg,
                                                        ShaderData *sd,
                                                        PathState *state,
                                                        Ray *ray,
                                                        float3 *throughput,
                                                        ccl_addr_space Intersection *isect,
                                                        bool hit,
                                                        ShaderData *indirect_sd,
                                                        ShaderData *emission_sd,
                                                        PathRadiance *L)
{
  /* Sanitize volume stack. */
  if (!hit) {
    kernel_volume_clean_stack(kg, state->volume_stack);
  }

  if (state->volume_stack[0].shader == SHADER_NONE) {
    return;
  }

  /* volume attenuation, emission, scatter */
  Ray volume_ray = *ray;
  volume_ray.t = (hit) ? isect->t : FLT_MAX;

  bool heterogeneous = volume_stack_is_heterogeneous(kg, state->volume_stack);

#      ifdef __VOLUME_DECOUPLED__
  /* decoupled ray marching only supported on CPU */
  if (kernel_data.integrator.volume_decoupled) {
    /* cache steps along volume for repeated sampling */
    VolumeSegment volume_segment;

    shader_setup_from_volume(kg, sd, &volume_ray);
    kernel_volume_decoupled_record(kg, state, &volume_ray, sd, &volume_segment, heterogeneous);

    /* direct light sampling */
    if (volume_segment.closure_flag & SD_SCATTER) {
      volume_segment.sampling_method = volume_stack_sampling_method(kg, state->volume_stack);

      int all = kernel_data.integrator.sample_all_lights_direct;

      kernel_branched_path_volume_connect_light(
          kg, sd, emission_sd, *throughput, state, L, all, &volume_ray, &volume_segment);

      /* indirect light sampling */
      int num_samples = kernel_data.integrator.volume_samples;
      float num_samples_inv = 1.0f / num_samples;

      for (int j = 0; j < num_samples; j++) {
        PathState ps = *state;
        Ray pray = *ray;
        float3 tp = *throughput;

        /* branch RNG state */
        path_state_branch(&ps, j, num_samples);

        /* scatter sample. if we use distance sampling and take just one
         * sample for direct and indirect light, we could share this
         * computation, but makes code a bit complex */
        float rphase = path_state_rng_1D(kg, &ps, PRNG_PHASE_CHANNEL);
        float rscatter = path_state_rng_1D(kg, &ps, PRNG_SCATTER_DISTANCE);

        VolumeIntegrateResult result = kernel_volume_decoupled_scatter(
            kg, &ps, &pray, sd, &tp, rphase, rscatter, &volume_segment, NULL, false);

        if (result == VOLUME_PATH_SCATTERED &&
            kernel_path_volume_bounce(kg, sd, &tp, &ps, &L->state, &pray)) {
          kernel_path_indirect(kg, indirect_sd, emission_sd, &pray, tp * num_samples_inv, &ps, L);

          /* for render passes, sum and reset indirect light pass variables
           * for the next samples */
          path_radiance_sum_indirect(L);
          path_radiance_reset_indirect(L);
        }
      }
    }

    /* emission and transmittance */
    if (volume_segment.closure_flag & SD_EMISSION)
      path_radiance_accum_emission(L, state, *throughput, volume_segment.accum_emission);
    *throughput *= volume_segment.accum_transmittance;

    /* free cached steps */
    kernel_volume_decoupled_free(kg, &volume_segment);
  }
  else
#      endif /* __VOLUME_DECOUPLED__ */
  {
    /* GPU: no decoupled ray marching, scatter probalistically */
    int num_samples = kernel_data.integrator.volume_samples;
    float num_samples_inv = 1.0f / num_samples;

    /* todo: we should cache the shader evaluations from stepping
     * through the volume, for now we redo them multiple times */

    for (int j = 0; j < num_samples; j++) {
      PathState ps = *state;
      Ray pray = *ray;
      float3 tp = (*throughput) * num_samples_inv;

      /* branch RNG state */
      path_state_branch(&ps, j, num_samples);

      VolumeIntegrateResult result = kernel_volume_integrate(
          kg, &ps, sd, &volume_ray, L, &tp, heterogeneous);

#      ifdef __VOLUME_SCATTER__
      if (result == VOLUME_PATH_SCATTERED) {
        /* todo: support equiangular, MIS and all light sampling.
         * alternatively get decoupled ray marching working on the GPU */
        kernel_path_volume_connect_light(kg, sd, emission_sd, tp, state, L);

        if (kernel_path_volume_bounce(kg, sd, &tp, &ps, &L->state, &pray)) {
          kernel_path_indirect(kg, indirect_sd, emission_sd, &pray, tp, &ps, L);

          /* for render passes, sum and reset indirect light pass variables
           * for the next samples */
          path_radiance_sum_indirect(L);
          path_radiance_reset_indirect(L);
        }
      }
#      endif /* __VOLUME_SCATTER__ */
    }

    /* todo: avoid this calculation using decoupled ray marching */
    kernel_volume_shadow(kg, emission_sd, state, &volume_ray, throughput);
  }
}
#    endif /* __VOLUME__ */

/* bounce off surface and integrate indirect light */
ccl_device_noinline void kernel_branched_path_surface_indirect_light(KernelGlobals *kg,
                                                                     ShaderData *sd,
                                                                     ShaderData *indirect_sd,
                                                                     ShaderData *emission_sd,
                                                                     float3 throughput,
                                                                     float num_samples_adjust,
                                                                     PathState *state,
                                                                     PathRadiance *L)
{
  float sum_sample_weight = 0.0f;
#    ifdef __DENOISING_FEATURES__
  if (state->denoising_feature_weight > 0.0f) {
    for (int i = 0; i < sd->num_closure; i++) {
      const ShaderClosure *sc = &sd->closure[i];

      /* transparency is not handled here, but in outer loop */
      if (!CLOSURE_IS_BSDF(sc->type) || CLOSURE_IS_BSDF_TRANSPARENT(sc->type)) {
        continue;
      }

      sum_sample_weight += sc->sample_weight;
    }
  }
  else {
    sum_sample_weight = 1.0f;
  }
#    endif /* __DENOISING_FEATURES__ */

  for (int i = 0; i < sd->num_closure; i++) {
    const ShaderClosure *sc = &sd->closure[i];

    /* transparency is not handled here, but in outer loop */
    if (!CLOSURE_IS_BSDF(sc->type) || CLOSURE_IS_BSDF_TRANSPARENT(sc->type)) {
      continue;
    }

    int num_samples;

    if (CLOSURE_IS_BSDF_DIFFUSE(sc->type))
      num_samples = kernel_data.integrator.diffuse_samples;
    else if (CLOSURE_IS_BSDF_BSSRDF(sc->type))
      num_samples = 1;
    else if (CLOSURE_IS_BSDF_GLOSSY(sc->type))
      num_samples = kernel_data.integrator.glossy_samples;
    else
      num_samples = kernel_data.integrator.transmission_samples;

    num_samples = ceil_to_int(num_samples_adjust * num_samples);

    float num_samples_inv = num_samples_adjust / num_samples;

    for (int j = 0; j < num_samples; j++) {
      PathState ps = *state;
      float3 tp = throughput;
      Ray bsdf_ray;
#    ifdef __SHADOW_TRICKS__
      float shadow_transparency = L->shadow_transparency;
#    endif

      ps.rng_hash = cmj_hash(state->rng_hash, i);

      if (!kernel_branched_path_surface_bounce(
              kg, sd, sc, j, num_samples, &tp, &ps, &L->state, &bsdf_ray, sum_sample_weight)) {
        continue;
      }

      ps.rng_hash = state->rng_hash;

      kernel_path_indirect(kg, indirect_sd, emission_sd, &bsdf_ray, tp * num_samples_inv, &ps, L);

      /* for render passes, sum and reset indirect light pass variables
       * for the next samples */
      path_radiance_sum_indirect(L);
      path_radiance_reset_indirect(L);

#    ifdef __SHADOW_TRICKS__
      L->shadow_transparency = shadow_transparency;
#    endif
    }
  }
}

#    ifdef __SUBSURFACE__
ccl_device void kernel_branched_path_subsurface_scatter(KernelGlobals *kg,
                                                        ShaderData *sd,
                                                        ShaderData *indirect_sd,
                                                        ShaderData *emission_sd,
                                                        PathRadiance *L,
                                                        PathState *state,
                                                        Ray *ray,
                                                        float3 throughput)
{
  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sc = &sd->closure[i];

    if (!CLOSURE_IS_BSSRDF(sc->type))
      continue;

    /* set up random number generator */
    uint lcg_state = lcg_state_init(state, 0x68bc21eb);
    int num_samples = kernel_data.integrator.subsurface_samples * 3;
    float num_samples_inv = 1.0f / num_samples;
    uint bssrdf_rng_hash = cmj_hash(state->rng_hash, i);

    /* do subsurface scatter step with copy of shader data, this will
     * replace the BSSRDF with a diffuse BSDF closure */
    for (int j = 0; j < num_samples; j++) {
      PathState hit_state = *state;
      path_state_branch(&hit_state, j, num_samples);
      hit_state.rng_hash = bssrdf_rng_hash;

      LocalIntersection ss_isect;
      float bssrdf_u, bssrdf_v;
      path_state_rng_2D(kg, &hit_state, PRNG_BSDF_U, &bssrdf_u, &bssrdf_v);
      int num_hits = subsurface_scatter_multi_intersect(
          kg, &ss_isect, sd, &hit_state, sc, &lcg_state, bssrdf_u, bssrdf_v, true);

      hit_state.rng_offset += PRNG_BOUNCE_NUM;

#      ifdef __VOLUME__
      Ray volume_ray = *ray;
      bool need_update_volume_stack = kernel_data.integrator.use_volumes &&
                                      sd->object_flag & SD_OBJECT_INTERSECTS_VOLUME;
#      endif /* __VOLUME__ */

      /* compute lighting with the BSDF closure */
      for (int hit = 0; hit < num_hits; hit++) {
        ShaderData bssrdf_sd = *sd;
        Bssrdf *bssrdf = (Bssrdf *)sc;
        ClosureType bssrdf_type = sc->type;
        float bssrdf_roughness = bssrdf->roughness;
        subsurface_scatter_multi_setup(
            kg, &ss_isect, hit, &bssrdf_sd, &hit_state, bssrdf_type, bssrdf_roughness);

#      ifdef __VOLUME__
        if (need_update_volume_stack) {
          /* Setup ray from previous surface point to the new one. */
          float3 P = ray_offset(bssrdf_sd.P, -bssrdf_sd.Ng);
          volume_ray.D = normalize_len(P - volume_ray.P, &volume_ray.t);

          for (int k = 0; k < VOLUME_STACK_SIZE; k++) {
            hit_state.volume_stack[k] = state->volume_stack[k];
          }

          kernel_volume_stack_update_for_subsurface(
              kg, emission_sd, &volume_ray, hit_state.volume_stack);
        }
#      endif /* __VOLUME__ */

#      ifdef __EMISSION__
        /* direct light */
        if (kernel_data.integrator.use_direct_light) {
          int all = (kernel_data.integrator.sample_all_lights_direct) ||
                    (hit_state.flag & PATH_RAY_SHADOW_CATCHER);
          kernel_branched_path_surface_connect_light(
              kg, &bssrdf_sd, emission_sd, &hit_state, throughput, num_samples_inv, L, all);
        }
#      endif /* __EMISSION__ */

        /* indirect light */
        kernel_branched_path_surface_indirect_light(
            kg, &bssrdf_sd, indirect_sd, emission_sd, throughput, num_samples_inv, &hit_state, L);
      }
    }
  }
}
#    endif /* __SUBSURFACE__ */

ccl_device void kernel_branched_path_integrate(KernelGlobals *kg,
                                               uint rng_hash,
                                               int sample,
                                               Ray ray,
                                               ccl_global float *buffer,
                                               PathRadiance *L)
{
  /* initialize */
  float3 throughput = make_float3(1.0f, 1.0f, 1.0f);

  path_radiance_init(L, kernel_data.film.use_light_pass);

  /* shader data memory used for both volumes and surfaces, saves stack space */
  ShaderData sd;
  /* shader data used by emission, shadows, volume stacks, indirect path */
  ShaderDataTinyStorage emission_sd_storage;
  ShaderData *emission_sd = AS_SHADER_DATA(&emission_sd_storage);
  ShaderData indirect_sd;

  PathState state;
  path_state_init(kg, emission_sd, &state, rng_hash, sample, &ray);

  /* Main Loop
   * Here we only handle transparency intersections from the camera ray.
   * Indirect bounces are handled in kernel_branched_path_surface_indirect_light().
   */
  for (;;) {
    /* Find intersection with objects in scene. */
    Intersection isect;
    bool hit = kernel_path_scene_intersect(kg, &state, &ray, &isect, L);

#    ifdef __VOLUME__
    /* Volume integration. */
    kernel_branched_path_volume(
        kg, &sd, &state, &ray, &throughput, &isect, hit, &indirect_sd, emission_sd, L);
#    endif /* __VOLUME__ */

    /* Shade background. */
    if (!hit) {
      kernel_path_background(kg, &state, &ray, throughput, &sd, L);
      break;
    }

    /* Setup and evaluate shader. */
    shader_setup_from_ray(kg, &sd, &isect, &ray);

    /* Skip most work for volume bounding surface. */
#    ifdef __VOLUME__
    if (!(sd.flag & SD_HAS_ONLY_VOLUME)) {
#    endif

      shader_eval_surface(kg, &sd, &state, state.flag);
      shader_merge_closures(&sd);

      /* Apply shadow catcher, holdout, emission. */
      if (!kernel_path_shader_apply(kg, &sd, &state, &ray, throughput, emission_sd, L, buffer)) {
        break;
      }

      /* transparency termination */
      if (state.flag & PATH_RAY_TRANSPARENT) {
        /* path termination. this is a strange place to put the termination, it's
         * mainly due to the mixed in MIS that we use. gives too many unneeded
         * shader evaluations, only need emission if we are going to terminate */
        float probability = path_state_continuation_probability(kg, &state, throughput);

        if (probability == 0.0f) {
          break;
        }
        else if (probability != 1.0f) {
          float terminate = path_state_rng_1D(kg, &state, PRNG_TERMINATE);

          if (terminate >= probability)
            break;

          throughput /= probability;
        }
      }

      kernel_update_denoising_features(kg, &sd, &state, L);

#    ifdef __AO__
      /* ambient occlusion */
      if (kernel_data.integrator.use_ambient_occlusion) {
        kernel_branched_path_ao(kg, &sd, emission_sd, L, &state, throughput);
      }
#    endif /* __AO__ */

#    ifdef __SUBSURFACE__
      /* bssrdf scatter to a different location on the same object */
      if (sd.flag & SD_BSSRDF) {
        kernel_branched_path_subsurface_scatter(
            kg, &sd, &indirect_sd, emission_sd, L, &state, &ray, throughput);
      }
#    endif /* __SUBSURFACE__ */

      PathState hit_state = state;

#    ifdef __EMISSION__
      /* direct light */
      if (kernel_data.integrator.use_direct_light) {
        int all = (kernel_data.integrator.sample_all_lights_direct) ||
                  (state.flag & PATH_RAY_SHADOW_CATCHER);
        kernel_branched_path_surface_connect_light(
            kg, &sd, emission_sd, &hit_state, throughput, 1.0f, L, all);
      }
#    endif /* __EMISSION__ */

      /* indirect light */
      kernel_branched_path_surface_indirect_light(
          kg, &sd, &indirect_sd, emission_sd, throughput, 1.0f, &hit_state, L);

      /* continue in case of transparency */
      throughput *= shader_bsdf_transparency(kg, &sd);

      if (is_zero(throughput))
        break;

      /* Update Path State */
      path_state_next(kg, &state, LABEL_TRANSPARENT);

#    ifdef __VOLUME__
    }
    else {
      if (!path_state_volume_next(kg, &state)) {
        break;
      }
    }
#    endif

    ray.P = ray_offset(sd.P, -sd.Ng);
    ray.t -= sd.ray_length; /* clipping works through transparent */

#    ifdef __RAY_DIFFERENTIALS__
    ray.dP = sd.dP;
    ray.dD.dx = -sd.dI.dx;
    ray.dD.dy = -sd.dI.dy;
#    endif /* __RAY_DIFFERENTIALS__ */

#    ifdef __VOLUME__
    /* enter/exit volume */
    kernel_volume_stack_enter_exit(kg, &sd, state.volume_stack);
#    endif /* __VOLUME__ */
  }
}

ccl_device void kernel_branched_path_trace(
    KernelGlobals *kg, ccl_global float *buffer, int sample, int x, int y, int offset, int stride)
{
  /* buffer offset */
  int index = offset + x + y * stride;
  int pass_stride = kernel_data.film.pass_stride;

  buffer += index * pass_stride;

  /* initialize random numbers and ray */
  uint rng_hash;
  Ray ray;

  kernel_path_trace_setup(kg, sample, x, y, &rng_hash, &ray);

  /* integrate */
  PathRadiance L;

  if (ray.t != 0.0f) {
    kernel_branched_path_integrate(kg, rng_hash, sample, ray, buffer, &L);
    kernel_write_result(kg, buffer, sample, &L);
  }
}

#  endif /* __SPLIT_KERNEL__ */

#endif /* __BRANCHED_PATH__ */

CCL_NAMESPACE_END
