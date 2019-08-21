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

/* BSDF Eval
 *
 * BSDF evaluation result, split per BSDF type. This is used to accumulate
 * render passes separately. */

ccl_device float3 shader_bsdf_transparency(KernelGlobals *kg, const ShaderData *sd);

ccl_device_inline void bsdf_eval_init(BsdfEval *eval,
                                      ClosureType type,
                                      float3 value,
                                      int use_light_pass)
{
#ifdef __PASSES__
  eval->use_light_pass = use_light_pass;

  if (eval->use_light_pass) {
    eval->diffuse = make_float3(0.0f, 0.0f, 0.0f);
    eval->glossy = make_float3(0.0f, 0.0f, 0.0f);
    eval->transmission = make_float3(0.0f, 0.0f, 0.0f);
    eval->transparent = make_float3(0.0f, 0.0f, 0.0f);
    eval->subsurface = make_float3(0.0f, 0.0f, 0.0f);
    eval->scatter = make_float3(0.0f, 0.0f, 0.0f);

    if (type == CLOSURE_BSDF_TRANSPARENT_ID)
      eval->transparent = value;
    else if (CLOSURE_IS_BSDF_DIFFUSE(type))
      eval->diffuse = value;
    else if (CLOSURE_IS_BSDF_GLOSSY(type))
      eval->glossy = value;
    else if (CLOSURE_IS_BSDF_TRANSMISSION(type))
      eval->transmission = value;
    else if (CLOSURE_IS_BSDF_BSSRDF(type))
      eval->subsurface = value;
    else if (CLOSURE_IS_PHASE(type))
      eval->scatter = value;
  }
  else
#endif
  {
    eval->diffuse = value;
  }
#ifdef __SHADOW_TRICKS__
  eval->sum_no_mis = make_float3(0.0f, 0.0f, 0.0f);
#endif
}

ccl_device_inline void bsdf_eval_accum(BsdfEval *eval,
                                       ClosureType type,
                                       float3 value,
                                       float mis_weight)
{
#ifdef __SHADOW_TRICKS__
  eval->sum_no_mis += value;
#endif
  value *= mis_weight;
#ifdef __PASSES__
  if (eval->use_light_pass) {
    if (CLOSURE_IS_BSDF_DIFFUSE(type))
      eval->diffuse += value;
    else if (CLOSURE_IS_BSDF_GLOSSY(type))
      eval->glossy += value;
    else if (CLOSURE_IS_BSDF_TRANSMISSION(type))
      eval->transmission += value;
    else if (CLOSURE_IS_BSDF_BSSRDF(type))
      eval->subsurface += value;
    else if (CLOSURE_IS_PHASE(type))
      eval->scatter += value;

    /* skipping transparent, this function is used by for eval(), will be zero then */
  }
  else
#endif
  {
    eval->diffuse += value;
  }
}

ccl_device_inline bool bsdf_eval_is_zero(BsdfEval *eval)
{
#ifdef __PASSES__
  if (eval->use_light_pass) {
    return is_zero(eval->diffuse) && is_zero(eval->glossy) && is_zero(eval->transmission) &&
           is_zero(eval->transparent) && is_zero(eval->subsurface) && is_zero(eval->scatter);
  }
  else
#endif
  {
    return is_zero(eval->diffuse);
  }
}

ccl_device_inline void bsdf_eval_mis(BsdfEval *eval, float value)
{
#ifdef __PASSES__
  if (eval->use_light_pass) {
    eval->diffuse *= value;
    eval->glossy *= value;
    eval->transmission *= value;
    eval->subsurface *= value;
    eval->scatter *= value;

    /* skipping transparent, this function is used by for eval(), will be zero then */
  }
  else
#endif
  {
    eval->diffuse *= value;
  }
}

ccl_device_inline void bsdf_eval_mul(BsdfEval *eval, float value)
{
#ifdef __SHADOW_TRICKS__
  eval->sum_no_mis *= value;
#endif
  bsdf_eval_mis(eval, value);
}

ccl_device_inline void bsdf_eval_mul3(BsdfEval *eval, float3 value)
{
#ifdef __SHADOW_TRICKS__
  eval->sum_no_mis *= value;
#endif
#ifdef __PASSES__
  if (eval->use_light_pass) {
    eval->diffuse *= value;
    eval->glossy *= value;
    eval->transmission *= value;
    eval->subsurface *= value;
    eval->scatter *= value;

    /* skipping transparent, this function is used by for eval(), will be zero then */
  }
  else
    eval->diffuse *= value;
#else
  eval->diffuse *= value;
#endif
}

ccl_device_inline float3 bsdf_eval_sum(const BsdfEval *eval)
{
#ifdef __PASSES__
  if (eval->use_light_pass) {
    return eval->diffuse + eval->glossy + eval->transmission + eval->subsurface + eval->scatter;
  }
  else
#endif
    return eval->diffuse;
}

/* Path Radiance
 *
 * We accumulate different render passes separately. After summing at the end
 * to get the combined result, it should be identical. We definite directly
 * visible as the first non-transparent hit, while indirectly visible are the
 * bounces after that. */

ccl_device_inline void path_radiance_init(PathRadiance *L, int use_light_pass)
{
  /* clear all */
#ifdef __PASSES__
  L->use_light_pass = use_light_pass;

  if (use_light_pass) {
    L->indirect = make_float3(0.0f, 0.0f, 0.0f);
    L->direct_emission = make_float3(0.0f, 0.0f, 0.0f);

    L->color_diffuse = make_float3(0.0f, 0.0f, 0.0f);
    L->color_glossy = make_float3(0.0f, 0.0f, 0.0f);
    L->color_transmission = make_float3(0.0f, 0.0f, 0.0f);
    L->color_subsurface = make_float3(0.0f, 0.0f, 0.0f);

    L->direct_diffuse = make_float3(0.0f, 0.0f, 0.0f);
    L->direct_glossy = make_float3(0.0f, 0.0f, 0.0f);
    L->direct_transmission = make_float3(0.0f, 0.0f, 0.0f);
    L->direct_subsurface = make_float3(0.0f, 0.0f, 0.0f);
    L->direct_scatter = make_float3(0.0f, 0.0f, 0.0f);

    L->indirect_diffuse = make_float3(0.0f, 0.0f, 0.0f);
    L->indirect_glossy = make_float3(0.0f, 0.0f, 0.0f);
    L->indirect_transmission = make_float3(0.0f, 0.0f, 0.0f);
    L->indirect_subsurface = make_float3(0.0f, 0.0f, 0.0f);
    L->indirect_scatter = make_float3(0.0f, 0.0f, 0.0f);

    L->transparent = 0.0f;
    L->emission = make_float3(0.0f, 0.0f, 0.0f);
    L->background = make_float3(0.0f, 0.0f, 0.0f);
    L->ao = make_float3(0.0f, 0.0f, 0.0f);
    L->shadow = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
    L->mist = 0.0f;

    L->state.diffuse = make_float3(0.0f, 0.0f, 0.0f);
    L->state.glossy = make_float3(0.0f, 0.0f, 0.0f);
    L->state.transmission = make_float3(0.0f, 0.0f, 0.0f);
    L->state.subsurface = make_float3(0.0f, 0.0f, 0.0f);
    L->state.scatter = make_float3(0.0f, 0.0f, 0.0f);
    L->state.direct = make_float3(0.0f, 0.0f, 0.0f);
  }
  else
#endif
  {
    L->transparent = 0.0f;
    L->emission = make_float3(0.0f, 0.0f, 0.0f);
  }

#ifdef __SHADOW_TRICKS__
  L->path_total = make_float3(0.0f, 0.0f, 0.0f);
  L->path_total_shaded = make_float3(0.0f, 0.0f, 0.0f);
  L->shadow_background_color = make_float3(0.0f, 0.0f, 0.0f);
  L->shadow_throughput = 0.0f;
  L->shadow_transparency = 1.0f;
  L->has_shadow_catcher = 0;
#endif

#ifdef __DENOISING_FEATURES__
  L->denoising_normal = make_float3(0.0f, 0.0f, 0.0f);
  L->denoising_albedo = make_float3(0.0f, 0.0f, 0.0f);
  L->denoising_depth = 0.0f;
#endif

#ifdef __KERNEL_DEBUG__
  L->debug_data.num_bvh_traversed_nodes = 0;
  L->debug_data.num_bvh_traversed_instances = 0;
  L->debug_data.num_bvh_intersections = 0;
  L->debug_data.num_ray_bounces = 0;
#endif
}

ccl_device_inline void path_radiance_bsdf_bounce(KernelGlobals *kg,
                                                 PathRadianceState *L_state,
                                                 ccl_addr_space float3 *throughput,
                                                 BsdfEval *bsdf_eval,
                                                 float bsdf_pdf,
                                                 int bounce,
                                                 int bsdf_label)
{
  float inverse_pdf = 1.0f / bsdf_pdf;

#ifdef __PASSES__
  if (kernel_data.film.use_light_pass) {
    if (bounce == 0 && !(bsdf_label & LABEL_TRANSPARENT)) {
      /* first on directly visible surface */
      float3 value = *throughput * inverse_pdf;

      L_state->diffuse = bsdf_eval->diffuse * value;
      L_state->glossy = bsdf_eval->glossy * value;
      L_state->transmission = bsdf_eval->transmission * value;
      L_state->subsurface = bsdf_eval->subsurface * value;
      L_state->scatter = bsdf_eval->scatter * value;

      *throughput = L_state->diffuse + L_state->glossy + L_state->transmission +
                    L_state->subsurface + L_state->scatter;

      L_state->direct = *throughput;
    }
    else {
      /* transparent bounce before first hit, or indirectly visible through BSDF */
      float3 sum = (bsdf_eval_sum(bsdf_eval) + bsdf_eval->transparent) * inverse_pdf;
      *throughput *= sum;
    }
  }
  else
#endif
  {
    *throughput *= bsdf_eval->diffuse * inverse_pdf;
  }
}

ccl_device_inline void path_radiance_accum_emission(PathRadiance *L,
                                                    ccl_addr_space PathState *state,
                                                    float3 throughput,
                                                    float3 value)
{
#ifdef __SHADOW_TRICKS__
  if (state->flag & PATH_RAY_SHADOW_CATCHER) {
    return;
  }
#endif

#ifdef __PASSES__
  if (L->use_light_pass) {
    if (state->bounce == 0)
      L->emission += throughput * value;
    else if (state->bounce == 1)
      L->direct_emission += throughput * value;
    else
      L->indirect += throughput * value;
  }
  else
#endif
  {
    L->emission += throughput * value;
  }
}

ccl_device_inline void path_radiance_accum_ao(PathRadiance *L,
                                              ccl_addr_space PathState *state,
                                              float3 throughput,
                                              float3 alpha,
                                              float3 bsdf,
                                              float3 ao)
{
#ifdef __PASSES__
  /* Store AO pass. */
  if (L->use_light_pass && state->bounce == 0) {
    L->ao += alpha * throughput * ao;
  }
#endif

#ifdef __SHADOW_TRICKS__
  /* For shadow catcher, accumulate ratio. */
  if (state->flag & PATH_RAY_STORE_SHADOW_INFO) {
    float3 light = throughput * bsdf;
    L->path_total += light;
    L->path_total_shaded += ao * light;

    if (state->flag & PATH_RAY_SHADOW_CATCHER) {
      return;
    }
  }
#endif

#ifdef __PASSES__
  if (L->use_light_pass) {
    if (state->bounce == 0) {
      /* Directly visible lighting. */
      L->direct_diffuse += throughput * bsdf * ao;
    }
    else {
      /* Indirectly visible lighting after BSDF bounce. */
      L->indirect += throughput * bsdf * ao;
    }
  }
  else
#endif
  {
    L->emission += throughput * bsdf * ao;
  }
}

ccl_device_inline void path_radiance_accum_total_ao(PathRadiance *L,
                                                    ccl_addr_space PathState *state,
                                                    float3 throughput,
                                                    float3 bsdf)
{
#ifdef __SHADOW_TRICKS__
  if (state->flag & PATH_RAY_STORE_SHADOW_INFO) {
    L->path_total += throughput * bsdf;
  }
#else
  (void)L;
  (void)state;
  (void)throughput;
  (void)bsdf;
#endif
}

ccl_device_inline void path_radiance_accum_light(PathRadiance *L,
                                                 ccl_addr_space PathState *state,
                                                 float3 throughput,
                                                 BsdfEval *bsdf_eval,
                                                 float3 shadow,
                                                 float shadow_fac,
                                                 bool is_lamp)
{
#ifdef __SHADOW_TRICKS__
  if (state->flag & PATH_RAY_STORE_SHADOW_INFO) {
    float3 light = throughput * bsdf_eval->sum_no_mis;
    L->path_total += light;
    L->path_total_shaded += shadow * light;

    if (state->flag & PATH_RAY_SHADOW_CATCHER) {
      return;
    }
  }
#endif

#ifdef __PASSES__
  if (L->use_light_pass) {
    if (state->bounce == 0) {
      /* directly visible lighting */
      L->direct_diffuse += throughput * bsdf_eval->diffuse * shadow;
      L->direct_glossy += throughput * bsdf_eval->glossy * shadow;
      L->direct_transmission += throughput * bsdf_eval->transmission * shadow;
      L->direct_subsurface += throughput * bsdf_eval->subsurface * shadow;
      L->direct_scatter += throughput * bsdf_eval->scatter * shadow;

      if (is_lamp) {
        L->shadow.x += shadow.x * shadow_fac;
        L->shadow.y += shadow.y * shadow_fac;
        L->shadow.z += shadow.z * shadow_fac;
      }
    }
    else {
      /* indirectly visible lighting after BSDF bounce */
      L->indirect += throughput * bsdf_eval_sum(bsdf_eval) * shadow;
    }
  }
  else
#endif
  {
    L->emission += throughput * bsdf_eval->diffuse * shadow;
  }
}

ccl_device_inline void path_radiance_accum_total_light(PathRadiance *L,
                                                       ccl_addr_space PathState *state,
                                                       float3 throughput,
                                                       const BsdfEval *bsdf_eval)
{
#ifdef __SHADOW_TRICKS__
  if (state->flag & PATH_RAY_STORE_SHADOW_INFO) {
    L->path_total += throughput * bsdf_eval->sum_no_mis;
  }
#else
  (void)L;
  (void)state;
  (void)throughput;
  (void)bsdf_eval;
#endif
}

ccl_device_inline void path_radiance_accum_background(PathRadiance *L,
                                                      ccl_addr_space PathState *state,
                                                      float3 throughput,
                                                      float3 value)
{

#ifdef __SHADOW_TRICKS__
  if (state->flag & PATH_RAY_STORE_SHADOW_INFO) {
    L->path_total += throughput * value;
    L->path_total_shaded += throughput * value * L->shadow_transparency;

    if (state->flag & PATH_RAY_SHADOW_CATCHER) {
      return;
    }
  }
#endif

#ifdef __PASSES__
  if (L->use_light_pass) {
    if (state->flag & PATH_RAY_TRANSPARENT_BACKGROUND)
      L->background += throughput * value;
    else if (state->bounce == 1)
      L->direct_emission += throughput * value;
    else
      L->indirect += throughput * value;
  }
  else
#endif
  {
    L->emission += throughput * value;
  }

#ifdef __DENOISING_FEATURES__
  L->denoising_albedo += state->denoising_feature_weight * value;
#endif /* __DENOISING_FEATURES__ */
}

ccl_device_inline void path_radiance_accum_transparent(PathRadiance *L,
                                                       ccl_addr_space PathState *state,
                                                       float3 throughput)
{
  L->transparent += average(throughput);
}

#ifdef __SHADOW_TRICKS__
ccl_device_inline void path_radiance_accum_shadowcatcher(PathRadiance *L,
                                                         float3 throughput,
                                                         float3 background)
{
  L->shadow_throughput += average(throughput);
  L->shadow_background_color += throughput * background;
  L->has_shadow_catcher = 1;
}
#endif

ccl_device_inline void path_radiance_sum_indirect(PathRadiance *L)
{
#ifdef __PASSES__
  /* this division is a bit ugly, but means we only have to keep track of
   * only a single throughput further along the path, here we recover just
   * the indirect path that is not influenced by any particular BSDF type */
  if (L->use_light_pass) {
    L->direct_emission = safe_divide_color(L->direct_emission, L->state.direct);
    L->direct_diffuse += L->state.diffuse * L->direct_emission;
    L->direct_glossy += L->state.glossy * L->direct_emission;
    L->direct_transmission += L->state.transmission * L->direct_emission;
    L->direct_subsurface += L->state.subsurface * L->direct_emission;
    L->direct_scatter += L->state.scatter * L->direct_emission;

    L->indirect = safe_divide_color(L->indirect, L->state.direct);
    L->indirect_diffuse += L->state.diffuse * L->indirect;
    L->indirect_glossy += L->state.glossy * L->indirect;
    L->indirect_transmission += L->state.transmission * L->indirect;
    L->indirect_subsurface += L->state.subsurface * L->indirect;
    L->indirect_scatter += L->state.scatter * L->indirect;
  }
#endif
}

ccl_device_inline void path_radiance_reset_indirect(PathRadiance *L)
{
#ifdef __PASSES__
  if (L->use_light_pass) {
    L->state.diffuse = make_float3(0.0f, 0.0f, 0.0f);
    L->state.glossy = make_float3(0.0f, 0.0f, 0.0f);
    L->state.transmission = make_float3(0.0f, 0.0f, 0.0f);
    L->state.subsurface = make_float3(0.0f, 0.0f, 0.0f);
    L->state.scatter = make_float3(0.0f, 0.0f, 0.0f);

    L->direct_emission = make_float3(0.0f, 0.0f, 0.0f);
    L->indirect = make_float3(0.0f, 0.0f, 0.0f);
  }
#endif
}

ccl_device_inline void path_radiance_copy_indirect(PathRadiance *L, const PathRadiance *L_src)
{
#ifdef __PASSES__
  if (L->use_light_pass) {
    L->state = L_src->state;

    L->direct_emission = L_src->direct_emission;
    L->indirect = L_src->indirect;
  }
#endif
}

#ifdef __SHADOW_TRICKS__
ccl_device_inline void path_radiance_sum_shadowcatcher(KernelGlobals *kg,
                                                       PathRadiance *L,
                                                       float3 *L_sum,
                                                       float *alpha)
{
  /* Calculate current shadow of the path. */
  float path_total = average(L->path_total);
  float shadow;

  if (UNLIKELY(!isfinite_safe(path_total))) {
    kernel_assert(!"Non-finite total radiance along the path");
    shadow = 0.0f;
  }
  else if (path_total == 0.0f) {
    shadow = L->shadow_transparency;
  }
  else {
    float path_total_shaded = average(L->path_total_shaded);
    shadow = path_total_shaded / path_total;
  }

  /* Calculate final light sum and transparency for shadow catcher object. */
  if (kernel_data.background.transparent) {
    *alpha -= L->shadow_throughput * shadow;
  }
  else {
    L->shadow_background_color *= shadow;
    *L_sum += L->shadow_background_color;
  }
}
#endif

ccl_device_inline float3 path_radiance_clamp_and_sum(KernelGlobals *kg,
                                                     PathRadiance *L,
                                                     float *alpha)
{
  float3 L_sum;
  /* Light Passes are used */
#ifdef __PASSES__
  float3 L_direct, L_indirect;
  float clamp_direct = kernel_data.integrator.sample_clamp_direct;
  float clamp_indirect = kernel_data.integrator.sample_clamp_indirect;
  if (L->use_light_pass) {
    path_radiance_sum_indirect(L);

    L_direct = L->direct_diffuse + L->direct_glossy + L->direct_transmission +
               L->direct_subsurface + L->direct_scatter + L->emission;
    L_indirect = L->indirect_diffuse + L->indirect_glossy + L->indirect_transmission +
                 L->indirect_subsurface + L->indirect_scatter;

    if (!kernel_data.background.transparent)
      L_direct += L->background;

    L_sum = L_direct + L_indirect;
    float sum = fabsf((L_sum).x) + fabsf((L_sum).y) + fabsf((L_sum).z);

    /* Reject invalid value */
    if (!isfinite_safe(sum)) {
      kernel_assert(!"Non-finite sum in path_radiance_clamp_and_sum!");
      L_sum = make_float3(0.0f, 0.0f, 0.0f);

      L->direct_diffuse = make_float3(0.0f, 0.0f, 0.0f);
      L->direct_glossy = make_float3(0.0f, 0.0f, 0.0f);
      L->direct_transmission = make_float3(0.0f, 0.0f, 0.0f);
      L->direct_subsurface = make_float3(0.0f, 0.0f, 0.0f);
      L->direct_scatter = make_float3(0.0f, 0.0f, 0.0f);

      L->indirect_diffuse = make_float3(0.0f, 0.0f, 0.0f);
      L->indirect_glossy = make_float3(0.0f, 0.0f, 0.0f);
      L->indirect_transmission = make_float3(0.0f, 0.0f, 0.0f);
      L->indirect_subsurface = make_float3(0.0f, 0.0f, 0.0f);
      L->indirect_scatter = make_float3(0.0f, 0.0f, 0.0f);

      L->emission = make_float3(0.0f, 0.0f, 0.0f);
    }

    /* Clamp direct and indirect samples */
#  ifdef __CLAMP_SAMPLE__
    else if (sum > clamp_direct || sum > clamp_indirect) {
      float scale;

      /* Direct */
      float sum_direct = fabsf(L_direct.x) + fabsf(L_direct.y) + fabsf(L_direct.z);
      if (sum_direct > clamp_direct) {
        scale = clamp_direct / sum_direct;
        L_direct *= scale;

        L->direct_diffuse *= scale;
        L->direct_glossy *= scale;
        L->direct_transmission *= scale;
        L->direct_subsurface *= scale;
        L->direct_scatter *= scale;
        L->emission *= scale;
        L->background *= scale;
      }

      /* Indirect */
      float sum_indirect = fabsf(L_indirect.x) + fabsf(L_indirect.y) + fabsf(L_indirect.z);
      if (sum_indirect > clamp_indirect) {
        scale = clamp_indirect / sum_indirect;
        L_indirect *= scale;

        L->indirect_diffuse *= scale;
        L->indirect_glossy *= scale;
        L->indirect_transmission *= scale;
        L->indirect_subsurface *= scale;
        L->indirect_scatter *= scale;
      }

      /* Sum again, after clamping */
      L_sum = L_direct + L_indirect;
    }
#  endif
  }

  /* No Light Passes */
  else
#endif
  {
    L_sum = L->emission;

    /* Reject invalid value */
    float sum = fabsf((L_sum).x) + fabsf((L_sum).y) + fabsf((L_sum).z);
    if (!isfinite_safe(sum)) {
      kernel_assert(!"Non-finite final sum in path_radiance_clamp_and_sum!");
      L_sum = make_float3(0.0f, 0.0f, 0.0f);
    }
  }

  /* Compute alpha. */
  *alpha = 1.0f - L->transparent;

  /* Add shadow catcher contributions. */
#ifdef __SHADOW_TRICKS__
  if (L->has_shadow_catcher) {
    path_radiance_sum_shadowcatcher(kg, L, &L_sum, alpha);
  }
#endif /* __SHADOW_TRICKS__ */

  return L_sum;
}

ccl_device_inline void path_radiance_split_denoising(KernelGlobals *kg,
                                                     PathRadiance *L,
                                                     float3 *noisy,
                                                     float3 *clean)
{
#ifdef __PASSES__
  kernel_assert(L->use_light_pass);

  *clean = L->emission + L->background;
  *noisy = L->direct_scatter + L->indirect_scatter;

#  define ADD_COMPONENT(flag, component) \
    if (kernel_data.film.denoising_flags & flag) \
      *clean += component; \
    else \
      *noisy += component;

  ADD_COMPONENT(DENOISING_CLEAN_DIFFUSE_DIR, L->direct_diffuse);
  ADD_COMPONENT(DENOISING_CLEAN_DIFFUSE_IND, L->indirect_diffuse);
  ADD_COMPONENT(DENOISING_CLEAN_GLOSSY_DIR, L->direct_glossy);
  ADD_COMPONENT(DENOISING_CLEAN_GLOSSY_IND, L->indirect_glossy);
  ADD_COMPONENT(DENOISING_CLEAN_TRANSMISSION_DIR, L->direct_transmission);
  ADD_COMPONENT(DENOISING_CLEAN_TRANSMISSION_IND, L->indirect_transmission);
  ADD_COMPONENT(DENOISING_CLEAN_SUBSURFACE_DIR, L->direct_subsurface);
  ADD_COMPONENT(DENOISING_CLEAN_SUBSURFACE_IND, L->indirect_subsurface);
#  undef ADD_COMPONENT
#else
  *noisy = L->emission;
  *clean = make_float3(0.0f, 0.0f, 0.0f);
#endif

#ifdef __SHADOW_TRICKS__
  if (L->has_shadow_catcher) {
    *noisy += L->shadow_background_color;
  }
#endif

  *noisy = ensure_finite3(*noisy);
  *clean = ensure_finite3(*clean);
}

ccl_device_inline void path_radiance_accum_sample(PathRadiance *L, PathRadiance *L_sample)
{
#ifdef __SPLIT_KERNEL__
#  define safe_float3_add(f, v) \
    do { \
      ccl_global float *p = (ccl_global float *)(&(f)); \
      atomic_add_and_fetch_float(p + 0, (v).x); \
      atomic_add_and_fetch_float(p + 1, (v).y); \
      atomic_add_and_fetch_float(p + 2, (v).z); \
    } while (0)
#  define safe_float_add(f, v) atomic_add_and_fetch_float(&(f), (v))
#else
#  define safe_float3_add(f, v) (f) += (v)
#  define safe_float_add(f, v) (f) += (v)
#endif /* __SPLIT_KERNEL__ */

#ifdef __PASSES__
  safe_float3_add(L->direct_diffuse, L_sample->direct_diffuse);
  safe_float3_add(L->direct_glossy, L_sample->direct_glossy);
  safe_float3_add(L->direct_transmission, L_sample->direct_transmission);
  safe_float3_add(L->direct_subsurface, L_sample->direct_subsurface);
  safe_float3_add(L->direct_scatter, L_sample->direct_scatter);

  safe_float3_add(L->indirect_diffuse, L_sample->indirect_diffuse);
  safe_float3_add(L->indirect_glossy, L_sample->indirect_glossy);
  safe_float3_add(L->indirect_transmission, L_sample->indirect_transmission);
  safe_float3_add(L->indirect_subsurface, L_sample->indirect_subsurface);
  safe_float3_add(L->indirect_scatter, L_sample->indirect_scatter);

  safe_float3_add(L->background, L_sample->background);
  safe_float3_add(L->ao, L_sample->ao);
  safe_float3_add(L->shadow, L_sample->shadow);
  safe_float_add(L->mist, L_sample->mist);
#endif /* __PASSES__ */
  safe_float3_add(L->emission, L_sample->emission);

#undef safe_float_add
#undef safe_float3_add
}

CCL_NAMESPACE_END
