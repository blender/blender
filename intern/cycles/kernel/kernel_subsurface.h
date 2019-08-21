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

/* BSSRDF using disk based importance sampling.
 *
 * BSSRDF Importance Sampling, SIGGRAPH 2013
 * http://library.imageworks.com/pdfs/imageworks-library-BSSRDF-sampling.pdf
 */

ccl_device_inline float3
subsurface_scatter_eval(ShaderData *sd, const ShaderClosure *sc, float disk_r, float r, bool all)
{
  /* this is the veach one-sample model with balance heuristic, some pdf
   * factors drop out when using balance heuristic weighting */
  float3 eval_sum = make_float3(0.0f, 0.0f, 0.0f);
  float pdf_sum = 0.0f;
  float sample_weight_inv = 0.0f;

  if (!all) {
    float sample_weight_sum = 0.0f;

    for (int i = 0; i < sd->num_closure; i++) {
      sc = &sd->closure[i];

      if (CLOSURE_IS_DISK_BSSRDF(sc->type)) {
        sample_weight_sum += sc->sample_weight;
      }
    }

    sample_weight_inv = 1.0f / sample_weight_sum;
  }

  for (int i = 0; i < sd->num_closure; i++) {
    sc = &sd->closure[i];

    if (CLOSURE_IS_DISK_BSSRDF(sc->type)) {
      /* in case of branched path integrate we sample all bssrdf's once,
       * for path trace we pick one, so adjust pdf for that */
      float sample_weight = (all) ? 1.0f : sc->sample_weight * sample_weight_inv;

      /* compute pdf */
      float3 eval = bssrdf_eval(sc, r);
      float pdf = bssrdf_pdf(sc, disk_r);

      eval_sum += sc->weight * eval;
      pdf_sum += sample_weight * pdf;
    }
  }

  return (pdf_sum > 0.0f) ? eval_sum / pdf_sum : make_float3(0.0f, 0.0f, 0.0f);
}

/* replace closures with a single diffuse bsdf closure after scatter step */
ccl_device void subsurface_scatter_setup_diffuse_bsdf(
    KernelGlobals *kg, ShaderData *sd, ClosureType type, float roughness, float3 weight, float3 N)
{
  sd->flag &= ~SD_CLOSURE_FLAGS;
  sd->num_closure = 0;
  sd->num_closure_left = kernel_data.integrator.max_closures;

#ifdef __PRINCIPLED__
  if (type == CLOSURE_BSSRDF_PRINCIPLED_ID || type == CLOSURE_BSSRDF_PRINCIPLED_RANDOM_WALK_ID) {
    PrincipledDiffuseBsdf *bsdf = (PrincipledDiffuseBsdf *)bsdf_alloc(
        sd, sizeof(PrincipledDiffuseBsdf), weight);

    if (bsdf) {
      bsdf->N = N;
      bsdf->roughness = roughness;
      sd->flag |= bsdf_principled_diffuse_setup(bsdf);

      /* replace CLOSURE_BSDF_PRINCIPLED_DIFFUSE_ID with this special ID so render passes
       * can recognize it as not being a regular Disney principled diffuse closure */
      bsdf->type = CLOSURE_BSDF_BSSRDF_PRINCIPLED_ID;
    }
  }
  else if (CLOSURE_IS_BSDF_BSSRDF(type) || CLOSURE_IS_BSSRDF(type))
#endif /* __PRINCIPLED__ */
  {
    DiffuseBsdf *bsdf = (DiffuseBsdf *)bsdf_alloc(sd, sizeof(DiffuseBsdf), weight);

    if (bsdf) {
      bsdf->N = N;
      sd->flag |= bsdf_diffuse_setup(bsdf);

      /* replace CLOSURE_BSDF_DIFFUSE_ID with this special ID so render passes
       * can recognize it as not being a regular diffuse closure */
      bsdf->type = CLOSURE_BSDF_BSSRDF_ID;
    }
  }
}

/* optionally do blurring of color and/or bump mapping, at the cost of a shader evaluation */
ccl_device float3 subsurface_color_pow(float3 color, float exponent)
{
  color = max(color, make_float3(0.0f, 0.0f, 0.0f));

  if (exponent == 1.0f) {
    /* nothing to do */
  }
  else if (exponent == 0.5f) {
    color.x = sqrtf(color.x);
    color.y = sqrtf(color.y);
    color.z = sqrtf(color.z);
  }
  else {
    color.x = powf(color.x, exponent);
    color.y = powf(color.y, exponent);
    color.z = powf(color.z, exponent);
  }

  return color;
}

ccl_device void subsurface_color_bump_blur(
    KernelGlobals *kg, ShaderData *sd, ccl_addr_space PathState *state, float3 *eval, float3 *N)
{
  /* average color and texture blur at outgoing point */
  float texture_blur;
  float3 out_color = shader_bssrdf_sum(sd, NULL, &texture_blur);

  /* do we have bump mapping? */
  bool bump = (sd->flag & SD_HAS_BSSRDF_BUMP) != 0;

  if (bump || texture_blur > 0.0f) {
    /* average color and normal at incoming point */
    shader_eval_surface(kg, sd, state, state->flag);
    float3 in_color = shader_bssrdf_sum(sd, (bump) ? N : NULL, NULL);

    /* we simply divide out the average color and multiply with the average
     * of the other one. we could try to do this per closure but it's quite
     * tricky to match closures between shader evaluations, their number and
     * order may change, this is simpler */
    if (texture_blur > 0.0f) {
      out_color = subsurface_color_pow(out_color, texture_blur);
      in_color = subsurface_color_pow(in_color, texture_blur);

      *eval *= safe_divide_color(in_color, out_color);
    }
  }
}

/* Subsurface scattering step, from a point on the surface to other
 * nearby points on the same object.
 */
ccl_device_inline int subsurface_scatter_disk(KernelGlobals *kg,
                                              LocalIntersection *ss_isect,
                                              ShaderData *sd,
                                              const ShaderClosure *sc,
                                              uint *lcg_state,
                                              float disk_u,
                                              float disk_v,
                                              bool all)
{
  /* pick random axis in local frame and point on disk */
  float3 disk_N, disk_T, disk_B;
  float pick_pdf_N, pick_pdf_T, pick_pdf_B;

  disk_N = sd->Ng;
  make_orthonormals(disk_N, &disk_T, &disk_B);

  if (disk_v < 0.5f) {
    pick_pdf_N = 0.5f;
    pick_pdf_T = 0.25f;
    pick_pdf_B = 0.25f;
    disk_v *= 2.0f;
  }
  else if (disk_v < 0.75f) {
    float3 tmp = disk_N;
    disk_N = disk_T;
    disk_T = tmp;
    pick_pdf_N = 0.25f;
    pick_pdf_T = 0.5f;
    pick_pdf_B = 0.25f;
    disk_v = (disk_v - 0.5f) * 4.0f;
  }
  else {
    float3 tmp = disk_N;
    disk_N = disk_B;
    disk_B = tmp;
    pick_pdf_N = 0.25f;
    pick_pdf_T = 0.25f;
    pick_pdf_B = 0.5f;
    disk_v = (disk_v - 0.75f) * 4.0f;
  }

  /* sample point on disk */
  float phi = M_2PI_F * disk_v;
  float disk_height, disk_r;

  bssrdf_sample(sc, disk_u, &disk_r, &disk_height);

  float3 disk_P = (disk_r * cosf(phi)) * disk_T + (disk_r * sinf(phi)) * disk_B;

  /* create ray */
#ifdef __SPLIT_KERNEL__
  Ray ray_object = ss_isect->ray;
  Ray *ray = &ray_object;
#else
  Ray *ray = &ss_isect->ray;
#endif
  ray->P = sd->P + disk_N * disk_height + disk_P;
  ray->D = -disk_N;
  ray->t = 2.0f * disk_height;
  ray->dP = sd->dP;
  ray->dD = differential3_zero();
  ray->time = sd->time;

  /* intersect with the same object. if multiple intersections are found it
   * will use at most BSSRDF_MAX_HITS hits, a random subset of all hits */
  scene_intersect_local(kg, ray, ss_isect, sd->object, lcg_state, BSSRDF_MAX_HITS);
  int num_eval_hits = min(ss_isect->num_hits, BSSRDF_MAX_HITS);

  for (int hit = 0; hit < num_eval_hits; hit++) {
    /* Quickly retrieve P and Ng without setting up ShaderData. */
    float3 hit_P;
    if (sd->type & PRIMITIVE_TRIANGLE) {
      hit_P = triangle_refine_local(kg, sd, &ss_isect->hits[hit], ray);
    }
#ifdef __OBJECT_MOTION__
    else if (sd->type & PRIMITIVE_MOTION_TRIANGLE) {
      float3 verts[3];
      motion_triangle_vertices(kg,
                               sd->object,
                               kernel_tex_fetch(__prim_index, ss_isect->hits[hit].prim),
                               sd->time,
                               verts);
      hit_P = motion_triangle_refine_local(kg, sd, &ss_isect->hits[hit], ray, verts);
    }
#endif /* __OBJECT_MOTION__ */
    else {
      ss_isect->weight[hit] = make_float3(0.0f, 0.0f, 0.0f);
      continue;
    }

    float3 hit_Ng = ss_isect->Ng[hit];
    if (ss_isect->hits[hit].object != OBJECT_NONE) {
      object_normal_transform(kg, sd, &hit_Ng);
    }

    /* Probability densities for local frame axes. */
    float pdf_N = pick_pdf_N * fabsf(dot(disk_N, hit_Ng));
    float pdf_T = pick_pdf_T * fabsf(dot(disk_T, hit_Ng));
    float pdf_B = pick_pdf_B * fabsf(dot(disk_B, hit_Ng));

    /* Multiple importance sample between 3 axes, power heuristic
     * found to be slightly better than balance heuristic. pdf_N
     * in the MIS weight and denominator cancelled out. */
    float w = pdf_N / (sqr(pdf_N) + sqr(pdf_T) + sqr(pdf_B));
    if (ss_isect->num_hits > BSSRDF_MAX_HITS) {
      w *= ss_isect->num_hits / (float)BSSRDF_MAX_HITS;
    }

    /* Real distance to sampled point. */
    float r = len(hit_P - sd->P);

    /* Evaluate profiles. */
    float3 eval = subsurface_scatter_eval(sd, sc, disk_r, r, all) * w;

    ss_isect->weight[hit] = eval;
  }

#ifdef __SPLIT_KERNEL__
  ss_isect->ray = *ray;
#endif

  return num_eval_hits;
}

ccl_device_noinline void subsurface_scatter_multi_setup(KernelGlobals *kg,
                                                        LocalIntersection *ss_isect,
                                                        int hit,
                                                        ShaderData *sd,
                                                        ccl_addr_space PathState *state,
                                                        ClosureType type,
                                                        float roughness)
{
#ifdef __SPLIT_KERNEL__
  Ray ray_object = ss_isect->ray;
  Ray *ray = &ray_object;
#else
  Ray *ray = &ss_isect->ray;
#endif

  /* Workaround for AMD GPU OpenCL compiler. Most probably cache bypass issue. */
#if defined(__SPLIT_KERNEL__) && defined(__KERNEL_OPENCL_AMD__) && defined(__KERNEL_GPU__)
  kernel_split_params.dummy_sd_flag = sd->flag;
#endif

  /* Setup new shading point. */
  shader_setup_from_subsurface(kg, sd, &ss_isect->hits[hit], ray);

  /* Optionally blur colors and bump mapping. */
  float3 weight = ss_isect->weight[hit];
  float3 N = sd->N;
  subsurface_color_bump_blur(kg, sd, state, &weight, &N);

  /* Setup diffuse BSDF. */
  subsurface_scatter_setup_diffuse_bsdf(kg, sd, type, roughness, weight, N);
}

/* Random walk subsurface scattering.
 *
 * "Practical and Controllable Subsurface Scattering for Production Path
 *  Tracing". Matt Jen-Yuan Chiang, Peter Kutz, Brent Burley. SIGGRAPH 2016. */

ccl_device void subsurface_random_walk_remap(const float A,
                                             const float d,
                                             float *sigma_t,
                                             float *sigma_s)
{
  /* Compute attenuation and scattering coefficients from albedo. */
  const float a = 1.0f - expf(A * (-5.09406f + A * (2.61188f - A * 4.31805f)));
  const float s = 1.9f - A + 3.5f * sqr(A - 0.8f);

  *sigma_t = 1.0f / fmaxf(d * s, 1e-16f);
  *sigma_s = *sigma_t * a;
}

ccl_device void subsurface_random_walk_coefficients(const ShaderClosure *sc,
                                                    float3 *sigma_t,
                                                    float3 *sigma_s,
                                                    float3 *weight)
{
  const Bssrdf *bssrdf = (const Bssrdf *)sc;
  const float3 A = bssrdf->albedo;
  const float3 d = bssrdf->radius;
  float sigma_t_x, sigma_t_y, sigma_t_z;
  float sigma_s_x, sigma_s_y, sigma_s_z;

  subsurface_random_walk_remap(A.x, d.x, &sigma_t_x, &sigma_s_x);
  subsurface_random_walk_remap(A.y, d.y, &sigma_t_y, &sigma_s_y);
  subsurface_random_walk_remap(A.z, d.z, &sigma_t_z, &sigma_s_z);

  *sigma_t = make_float3(sigma_t_x, sigma_t_y, sigma_t_z);
  *sigma_s = make_float3(sigma_s_x, sigma_s_y, sigma_s_z);

  /* Closure mixing and Fresnel weights separate from albedo. */
  *weight = safe_divide_color(bssrdf->weight, A);
}

ccl_device_noinline bool subsurface_random_walk(KernelGlobals *kg,
                                                LocalIntersection *ss_isect,
                                                ShaderData *sd,
                                                ccl_addr_space PathState *state,
                                                const ShaderClosure *sc,
                                                const float bssrdf_u,
                                                const float bssrdf_v)
{
  /* Sample diffuse surface scatter into the object. */
  float3 D;
  float pdf;
  sample_cos_hemisphere(-sd->N, bssrdf_u, bssrdf_v, &D, &pdf);
  if (dot(-sd->Ng, D) <= 0.0f) {
    return 0;
  }

  /* Convert subsurface to volume coefficients. */
  float3 sigma_t, sigma_s;
  float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
  subsurface_random_walk_coefficients(sc, &sigma_t, &sigma_s, &throughput);

  /* Setup ray. */
#ifdef __SPLIT_KERNEL__
  Ray ray_object = ss_isect->ray;
  Ray *ray = &ray_object;
#else
  Ray *ray = &ss_isect->ray;
#endif
  ray->P = ray_offset(sd->P, -sd->Ng);
  ray->D = D;
  ray->t = FLT_MAX;
  ray->time = sd->time;

  /* Modify state for RNGs, decorrelated from other paths. */
  uint prev_rng_offset = state->rng_offset;
  uint prev_rng_hash = state->rng_hash;
  state->rng_hash = cmj_hash(state->rng_hash + state->rng_offset, 0xdeadbeef);

  /* Random walk until we hit the surface again. */
  bool hit = false;

  for (int bounce = 0; bounce < BSSRDF_MAX_BOUNCES; bounce++) {
    /* Advance random number offset. */
    state->rng_offset += PRNG_BOUNCE_NUM;

    if (bounce > 0) {
      /* Sample scattering direction. */
      const float anisotropy = 0.0f;
      float scatter_u, scatter_v;
      path_state_rng_2D(kg, state, PRNG_BSDF_U, &scatter_u, &scatter_v);
      ray->D = henyey_greenstrein_sample(ray->D, anisotropy, scatter_u, scatter_v, NULL);
    }

    /* Sample color channel, use MIS with balance heuristic. */
    float rphase = path_state_rng_1D(kg, state, PRNG_PHASE_CHANNEL);
    float3 albedo = safe_divide_color(sigma_s, sigma_t);
    float3 channel_pdf;
    int channel = kernel_volume_sample_channel(albedo, throughput, rphase, &channel_pdf);

    /* Distance sampling. */
    float rdist = path_state_rng_1D(kg, state, PRNG_SCATTER_DISTANCE);
    float sample_sigma_t = kernel_volume_channel_get(sigma_t, channel);
    float t = -logf(1.0f - rdist) / sample_sigma_t;

    ray->t = t;
    scene_intersect_local(kg, ray, ss_isect, sd->object, NULL, 1);
    hit = (ss_isect->num_hits > 0);

    if (hit) {
      /* Compute world space distance to surface hit. */
      float3 D = ray->D;
      object_inverse_dir_transform(kg, sd, &D);
      D = normalize(D) * ss_isect->hits[0].t;
      object_dir_transform(kg, sd, &D);
      t = len(D);
    }

    /* Advance to new scatter location. */
    ray->P += t * ray->D;

    /* Update throughput. */
    float3 transmittance = volume_color_transmittance(sigma_t, t);
    float pdf = dot(channel_pdf, (hit) ? transmittance : sigma_t * transmittance);
    throughput *= ((hit) ? transmittance : sigma_s * transmittance) / pdf;

    if (hit) {
      /* If we hit the surface, we are done. */
      break;
    }

    /* Russian roulette. */
    float terminate = path_state_rng_1D(kg, state, PRNG_TERMINATE);
    float probability = min(max3(fabs(throughput)), 1.0f);
    if (terminate >= probability) {
      break;
    }
    throughput /= probability;
  }

  kernel_assert(isfinite_safe(throughput.x) && isfinite_safe(throughput.y) &&
                isfinite_safe(throughput.z));

  state->rng_offset = prev_rng_offset;
  state->rng_hash = prev_rng_hash;

  /* Return number of hits in ss_isect. */
  if (!hit) {
    return 0;
  }

  /* TODO: gain back performance lost from merging with disk BSSRDF. We
   * only need to return on hit so this indirect ray push/pop overhead
   * is not actually needed, but it does keep the code simpler. */
  ss_isect->weight[0] = throughput;
#ifdef __SPLIT_KERNEL__
  ss_isect->ray = *ray;
#endif

  return 1;
}

ccl_device_inline int subsurface_scatter_multi_intersect(KernelGlobals *kg,
                                                         LocalIntersection *ss_isect,
                                                         ShaderData *sd,
                                                         ccl_addr_space PathState *state,
                                                         const ShaderClosure *sc,
                                                         uint *lcg_state,
                                                         float bssrdf_u,
                                                         float bssrdf_v,
                                                         bool all)
{
  if (CLOSURE_IS_DISK_BSSRDF(sc->type)) {
    return subsurface_scatter_disk(kg, ss_isect, sd, sc, lcg_state, bssrdf_u, bssrdf_v, all);
  }
  else {
    return subsurface_random_walk(kg, ss_isect, sd, state, sc, bssrdf_u, bssrdf_v);
  }
}

CCL_NAMESPACE_END
