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
  /* This is the Veach one-sample model with balance heuristic, some pdf
   * factors drop out when using balance heuristic weighting. For branched
   * path tracing (all) we sample all closure and don't use MIS. */
  float3 eval_sum = zero_float3();
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

  return (pdf_sum > 0.0f) ? eval_sum / pdf_sum : zero_float3();
}

ccl_device_inline float3 subsurface_scatter_walk_eval(ShaderData *sd,
                                                      const ShaderClosure *sc,
                                                      float3 throughput,
                                                      bool all)
{
  /* This is the Veach one-sample model with balance heuristic, some pdf
   * factors drop out when using balance heuristic weighting. For branched
   * path tracing (all) we sample all closure and don't use MIS. */
  if (!all) {
    float bssrdf_weight = 0.0f;
    float weight = sc->sample_weight;

    for (int i = 0; i < sd->num_closure; i++) {
      sc = &sd->closure[i];

      if (CLOSURE_IS_BSSRDF(sc->type)) {
        bssrdf_weight += sc->sample_weight;
      }
    }
    throughput *= bssrdf_weight / weight;
  }
  return throughput;
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
  color = max(color, zero_float3());

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
    shader_eval_surface(kg, sd, state, NULL, state->flag);
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
      ss_isect->weight[hit] = zero_float3();
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

#if defined(__KERNEL_OPTIX__) && defined(__SHADER_RAYTRACE__)
ccl_device_inline void subsurface_scatter_multi_setup(KernelGlobals *kg,
                                                      LocalIntersection *ss_isect,
                                                      int hit,
                                                      ShaderData *sd,
                                                      ccl_addr_space PathState *state,
                                                      ClosureType type,
                                                      float roughness)
{
  optixDirectCall<void>(2, kg, ss_isect, hit, sd, state, type, roughness);
}
extern "C" __device__ void __direct_callable__subsurface_scatter_multi_setup(
#else
ccl_device_noinline void subsurface_scatter_multi_setup(
#endif
    KernelGlobals *kg,
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
                                             float *alpha)
{
  /* Compute attenuation and scattering coefficients from albedo. */
  *alpha = 1.0f - expf(A * (-5.09406f + A * (2.61188f - A * 4.31805f)));
  const float s = 1.9f - A + 3.5f * sqr(A - 0.8f);

  *sigma_t = 1.0f / fmaxf(d * s, 1e-16f);
}

ccl_device void subsurface_random_walk_coefficients(const ShaderClosure *sc,
                                                    float3 *sigma_t,
                                                    float3 *alpha,
                                                    float3 *weight)
{
  const Bssrdf *bssrdf = (const Bssrdf *)sc;
  const float3 A = bssrdf->albedo;
  const float3 d = bssrdf->radius;
  float sigma_t_x, sigma_t_y, sigma_t_z;
  float alpha_x, alpha_y, alpha_z;

  subsurface_random_walk_remap(A.x, d.x, &sigma_t_x, &alpha_x);
  subsurface_random_walk_remap(A.y, d.y, &sigma_t_y, &alpha_y);
  subsurface_random_walk_remap(A.z, d.z, &sigma_t_z, &alpha_z);

  *sigma_t = make_float3(sigma_t_x, sigma_t_y, sigma_t_z);
  *alpha = make_float3(alpha_x, alpha_y, alpha_z);

  /* Closure mixing and Fresnel weights separate from albedo. */
  *weight = safe_divide_color(bssrdf->weight, A);
}

/* References for Dwivedi sampling:
 *
 * [1] "A Zero-variance-based Sampling Scheme for Monte Carlo Subsurface Scattering"
 * by Jaroslav Křivánek and Eugene d'Eon (SIGGRAPH 2014)
 * https://cgg.mff.cuni.cz/~jaroslav/papers/2014-zerovar/
 *
 * [2] "Improving the Dwivedi Sampling Scheme"
 * by Johannes Meng, Johannes Hanika, and Carsten Dachsbacher (EGSR 2016)
 * https://cg.ivd.kit.edu/1951.php
 *
 * [3] "Zero-Variance Theory for Efficient Subsurface Scattering"
 * by Eugene d'Eon and Jaroslav Křivánek (SIGGRAPH 2020)
 * https://iliyan.com/publications/RenderingCourse2020
 */

ccl_device_forceinline float eval_phase_dwivedi(float v, float phase_log, float cos_theta)
{
  /* Eq. 9 from [2] using precomputed log((v + 1) / (v - 1))*/
  return 1.0f / ((v - cos_theta) * phase_log);
}

ccl_device_forceinline float sample_phase_dwivedi(float v, float phase_log, float rand)
{
  /* Based on Eq. 10 from [2]: `v - (v + 1) * pow((v - 1) / (v + 1), rand)`
   * Since we're already pre-computing `phase_log = log((v + 1) / (v - 1))` for the evaluation,
   * we can implement the power function like this. */
  return v - (v + 1) * expf(-rand * phase_log);
}

ccl_device_forceinline float diffusion_length_dwivedi(float alpha)
{
  /* Eq. 67 from [3] */
  return 1.0f / sqrtf(1.0f - powf(alpha, 2.44294f - 0.0215813f * alpha + 0.578637f / alpha));
}

ccl_device_forceinline float3 direction_from_cosine(float3 D, float cos_theta, float randv)
{
  float sin_theta = safe_sqrtf(1.0f - cos_theta * cos_theta);
  float phi = M_2PI_F * randv;
  float3 dir = make_float3(sin_theta * cosf(phi), sin_theta * sinf(phi), cos_theta);

  float3 T, B;
  make_orthonormals(D, &T, &B);
  return dir.x * T + dir.y * B + dir.z * D;
}

ccl_device_forceinline float3 subsurface_random_walk_pdf(float3 sigma_t,
                                                         float t,
                                                         bool hit,
                                                         float3 *transmittance)
{
  float3 T = volume_color_transmittance(sigma_t, t);
  if (transmittance) {
    *transmittance = T;
  }
  return hit ? T : sigma_t * T;
}

#ifdef __KERNEL_OPTIX__
ccl_device_inline /* inline trace calls */
#else
ccl_device_noinline
#endif
    bool
    subsurface_random_walk(KernelGlobals *kg,
                           LocalIntersection *ss_isect,
                           ShaderData *sd,
                           ccl_addr_space PathState *state,
                           const ShaderClosure *sc,
                           const float bssrdf_u,
                           const float bssrdf_v,
                           bool all)
{
  /* Sample diffuse surface scatter into the object. */
  float3 D;
  float pdf;
  sample_cos_hemisphere(-sd->N, bssrdf_u, bssrdf_v, &D, &pdf);
  if (dot(-sd->Ng, D) <= 0.0f) {
    return 0;
  }

  /* Convert subsurface to volume coefficients.
   * The single-scattering albedo is named alpha to avoid confusion with the surface albedo. */
  float3 sigma_t, alpha;
  float3 throughput = one_float3();
  subsurface_random_walk_coefficients(sc, &sigma_t, &alpha, &throughput);
  float3 sigma_s = sigma_t * alpha;

  /* Theoretically it should be better to use the exact alpha for the channel we're sampling at
   * each bounce, but in practice there doesn't seem to be a noticeable difference in exchange
   * for making the code significantly more complex and slower (if direction sampling depends on
   * the sampled channel, we need to compute its PDF per-channel and consider it for MIS later on).
   *
   * Since the strength of the guided sampling increases as alpha gets lower, using a value that
   * is too low results in fireflies while one that's too high just gives a bit more noise.
   * Therefore, the code here uses the highest of the three albedos to be safe. */
  float diffusion_length = diffusion_length_dwivedi(max3(alpha));
  /* Precompute term for phase sampling. */
  float phase_log = logf((diffusion_length + 1) / (diffusion_length - 1));

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
  bool have_opposite_interface = false;
  float opposite_distance = 0.0f;

  /* Todo: Disable for alpha>0.999 or so? */
  const float guided_fraction = 0.75f;

  for (int bounce = 0; bounce < BSSRDF_MAX_BOUNCES; bounce++) {
    /* Advance random number offset. */
    state->rng_offset += PRNG_BOUNCE_NUM;

    /* Sample color channel, use MIS with balance heuristic. */
    float rphase = path_state_rng_1D(kg, state, PRNG_PHASE_CHANNEL);
    float3 channel_pdf;
    int channel = kernel_volume_sample_channel(alpha, throughput, rphase, &channel_pdf);
    float sample_sigma_t = kernel_volume_channel_get(sigma_t, channel);
    float randt = path_state_rng_1D(kg, state, PRNG_SCATTER_DISTANCE);

    /* We need the result of the raycast to compute the full guided PDF, so just remember the
     * relevant terms to avoid recomputing them later. */
    float backward_fraction = 0.0f;
    float forward_pdf_factor = 0.0f;
    float forward_stretching = 1.0f;
    float backward_pdf_factor = 0.0f;
    float backward_stretching = 1.0f;

    /* For the initial ray, we already know the direction, so just do classic distance sampling. */
    if (bounce > 0) {
      /* Decide whether we should use guided or classic sampling. */
      bool guided = (path_state_rng_1D(kg, state, PRNG_LIGHT_TERMINATE) < guided_fraction);

      /* Determine if we want to sample away from the incoming interface.
       * This only happens if we found a nearby opposite interface, and the probability for it
       * depends on how close we are to it already.
       * This probability term comes from the recorded presentation of [3]. */
      bool guide_backward = false;
      if (have_opposite_interface) {
        /* Compute distance of the random walk between the tangent plane at the starting point
         * and the assumed opposite interface (the parallel plane that contains the point we
         * found in our ray query for the opposite side). */
        float x = clamp(dot(ray->P - sd->P, -sd->N), 0.0f, opposite_distance);
        backward_fraction = 1.0f / (1.0f + expf((opposite_distance - 2 * x) / diffusion_length));
        guide_backward = path_state_rng_1D(kg, state, PRNG_TERMINATE) < backward_fraction;
      }

      /* Sample scattering direction. */
      float scatter_u, scatter_v;
      path_state_rng_2D(kg, state, PRNG_BSDF_U, &scatter_u, &scatter_v);
      float cos_theta;
      if (guided) {
        cos_theta = sample_phase_dwivedi(diffusion_length, phase_log, scatter_u);
        /* The backwards guiding distribution is just mirrored along sd->N, so swapping the
         * sign here is enough to sample from that instead. */
        if (guide_backward) {
          cos_theta = -cos_theta;
        }
      }
      else {
        cos_theta = 2.0f * scatter_u - 1.0f;
      }
      ray->D = direction_from_cosine(sd->N, cos_theta, scatter_v);

      /* Compute PDF factor caused by phase sampling (as the ratio of guided / classic).
       * Since phase sampling is channel-independent, we can get away with applying a factor
       * to the guided PDF, which implicitly means pulling out the classic PDF term and letting
       * it cancel with an equivalent term in the numerator of the full estimator.
       * For the backward PDF, we again reuse the same probability distribution with a sign swap.
       */
      forward_pdf_factor = 2.0f * eval_phase_dwivedi(diffusion_length, phase_log, cos_theta);
      backward_pdf_factor = 2.0f * eval_phase_dwivedi(diffusion_length, phase_log, -cos_theta);

      /* Prepare distance sampling.
       * For the backwards case, this also needs the sign swapped since now directions against
       * sd->N (and therefore with negative cos_theta) are preferred. */
      forward_stretching = (1.0f - cos_theta / diffusion_length);
      backward_stretching = (1.0f + cos_theta / diffusion_length);
      if (guided) {
        sample_sigma_t *= guide_backward ? backward_stretching : forward_stretching;
      }
    }

    /* Sample direction along ray. */
    float t = -logf(1.0f - randt) / sample_sigma_t;

    /* On the first bounce, we use the raycast to check if the opposite side is nearby.
     * If yes, we will later use backwards guided sampling in order to have a decent
     * chance of connecting to it.
     * Todo: Maybe use less than 10 times the mean free path? */
    ray->t = (bounce == 0) ? max(t, 10.0f / (min3(sigma_t))) : t;
    scene_intersect_local(kg, ray, ss_isect, sd->object, NULL, 1);
    hit = (ss_isect->num_hits > 0);

    if (hit) {
#ifdef __KERNEL_OPTIX__
      /* t is always in world space with OptiX. */
      ray->t = ss_isect->hits[0].t;
#else
      /* Compute world space distance to surface hit. */
      float3 D = ray->D;
      object_inverse_dir_transform(kg, sd, &D);
      D = normalize(D) * ss_isect->hits[0].t;
      object_dir_transform(kg, sd, &D);
      ray->t = len(D);
#endif
    }

    if (bounce == 0) {
      /* Check if we hit the opposite side. */
      if (hit) {
        have_opposite_interface = true;
        opposite_distance = dot(ray->P + ray->t * ray->D - sd->P, -sd->N);
      }
      /* Apart from the opposite side check, we were supposed to only trace up to distance t,
       * so check if there would have been a hit in that case. */
      hit = ray->t < t;
    }

    /* Use the distance to the exit point for the throughput update if we found one. */
    if (hit) {
      t = ray->t;
    }
    else if (bounce == 0) {
      /* Restore original position if nothing was hit after the first bounce,
       * without the ray_offset() that was added to avoid self-intersection.
       * Otherwise if that offset is relatively large compared to the scattering
       * radius, we never go back up high enough to exit the surface. */
      ray->P = sd->P;
    }

    /* Advance to new scatter location. */
    ray->P += t * ray->D;

    float3 transmittance;
    float3 pdf = subsurface_random_walk_pdf(sigma_t, t, hit, &transmittance);
    if (bounce > 0) {
      /* Compute PDF just like we do for classic sampling, but with the stretched sigma_t. */
      float3 guided_pdf = subsurface_random_walk_pdf(forward_stretching * sigma_t, t, hit, NULL);

      if (have_opposite_interface) {
        /* First step of MIS: Depending on geometry we might have two methods for guided
         * sampling, so perform MIS between them. */
        float3 back_pdf = subsurface_random_walk_pdf(backward_stretching * sigma_t, t, hit, NULL);
        guided_pdf = mix(
            guided_pdf * forward_pdf_factor, back_pdf * backward_pdf_factor, backward_fraction);
      }
      else {
        /* Just include phase sampling factor otherwise. */
        guided_pdf *= forward_pdf_factor;
      }

      /* Now we apply the MIS balance heuristic between the classic and guided sampling. */
      pdf = mix(pdf, guided_pdf, guided_fraction);
    }

    /* Finally, we're applying MIS again to combine the three color channels.
     * Altogether, the MIS computation combines up to nine different estimators:
     * {classic, guided, backward_guided} x {r, g, b} */
    throughput *= (hit ? transmittance : sigma_s * transmittance) / dot(channel_pdf, pdf);

    if (hit) {
      /* If we hit the surface, we are done. */
      break;
    }
    else if (throughput.x < VOLUME_THROUGHPUT_EPSILON &&
             throughput.y < VOLUME_THROUGHPUT_EPSILON &&
             throughput.z < VOLUME_THROUGHPUT_EPSILON) {
      /* Avoid unnecessary work and precision issue when throughput gets really small. */
      break;
    }
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
  ss_isect->weight[0] = subsurface_scatter_walk_eval(sd, sc, throughput, all);
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
    return subsurface_random_walk(kg, ss_isect, sd, state, sc, bssrdf_u, bssrdf_v, all);
  }
}

CCL_NAMESPACE_END
