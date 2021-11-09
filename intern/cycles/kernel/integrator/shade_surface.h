/*
 * Copyright 2011-2021 Blender Foundation
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

#pragma once

#include "kernel/film/accumulate.h"
#include "kernel/film/passes.h"

#include "kernel/integrator/path_state.h"
#include "kernel/integrator/shader_eval.h"
#include "kernel/integrator/subsurface.h"
#include "kernel/integrator/volume_stack.h"

#include "kernel/light/light.h"
#include "kernel/light/sample.h"

#include "kernel/sample/mis.h"

CCL_NAMESPACE_BEGIN

ccl_device_forceinline void integrate_surface_shader_setup(KernelGlobals kg,
                                                           ConstIntegratorState state,
                                                           ccl_private ShaderData *sd)
{
  Intersection isect ccl_optional_struct_init;
  integrator_state_read_isect(kg, state, &isect);

  Ray ray ccl_optional_struct_init;
  integrator_state_read_ray(kg, state, &ray);

  shader_setup_from_ray(kg, sd, &ray, &isect);
}

#ifdef __HOLDOUT__
ccl_device_forceinline bool integrate_surface_holdout(KernelGlobals kg,
                                                      ConstIntegratorState state,
                                                      ccl_private ShaderData *sd,
                                                      ccl_global float *ccl_restrict render_buffer)
{
  /* Write holdout transparency to render buffer and stop if fully holdout. */
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  if (((sd->flag & SD_HOLDOUT) || (sd->object_flag & SD_OBJECT_HOLDOUT_MASK)) &&
      (path_flag & PATH_RAY_TRANSPARENT_BACKGROUND)) {
    const float3 holdout_weight = shader_holdout_apply(kg, sd);
    if (kernel_data.background.transparent) {
      const float3 throughput = INTEGRATOR_STATE(state, path, throughput);
      const float transparent = average(holdout_weight * throughput);
      kernel_accum_holdout(kg, state, path_flag, transparent, render_buffer);
    }
    if (isequal_float3(holdout_weight, one_float3())) {
      return false;
    }
  }

  return true;
}
#endif /* __HOLDOUT__ */

#ifdef __EMISSION__
ccl_device_forceinline void integrate_surface_emission(KernelGlobals kg,
                                                       ConstIntegratorState state,
                                                       ccl_private const ShaderData *sd,
                                                       ccl_global float *ccl_restrict
                                                           render_buffer)
{
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  /* Evaluate emissive closure. */
  float3 L = shader_emissive_eval(sd);

#  ifdef __HAIR__
  if (!(path_flag & PATH_RAY_MIS_SKIP) && (sd->flag & SD_USE_MIS) &&
      (sd->type & PRIMITIVE_ALL_TRIANGLE))
#  else
  if (!(path_flag & PATH_RAY_MIS_SKIP) && (sd->flag & SD_USE_MIS))
#  endif
  {
    const float bsdf_pdf = INTEGRATOR_STATE(state, path, mis_ray_pdf);
    const float t = sd->ray_length + INTEGRATOR_STATE(state, path, mis_ray_t);

    /* Multiple importance sampling, get triangle light pdf,
     * and compute weight with respect to BSDF pdf. */
    float pdf = triangle_light_pdf(kg, sd, t);
    float mis_weight = power_heuristic(bsdf_pdf, pdf);

    L *= mis_weight;
  }

  const float3 throughput = INTEGRATOR_STATE(state, path, throughput);
  kernel_accum_emission(kg, state, throughput * L, render_buffer);
}
#endif /* __EMISSION__ */

#ifdef __EMISSION__
/* Path tracing: sample point on light and evaluate light shader, then
 * queue shadow ray to be traced. */
ccl_device_forceinline void integrate_surface_direct_light(KernelGlobals kg,
                                                           IntegratorState state,
                                                           ccl_private ShaderData *sd,
                                                           ccl_private const RNGState *rng_state)
{
  /* Test if there is a light or BSDF that needs direct light. */
  if (!(kernel_data.integrator.use_direct_light && (sd->flag & SD_BSDF_HAS_EVAL))) {
    return;
  }

  /* Sample position on a light. */
  LightSample ls ccl_optional_struct_init;
  {
    const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
    const uint bounce = INTEGRATOR_STATE(state, path, bounce);
    float light_u, light_v;
    path_state_rng_2D(kg, rng_state, PRNG_LIGHT_U, &light_u, &light_v);

    if (!light_distribution_sample_from_position(
            kg, light_u, light_v, sd->time, sd->P, bounce, path_flag, &ls)) {
      return;
    }
  }

  kernel_assert(ls.pdf != 0.0f);

  /* Evaluate light shader.
   *
   * TODO: can we reuse sd memory? In theory we can move this after
   * integrate_surface_bounce, evaluate the BSDF, and only then evaluate
   * the light shader. This could also move to its own kernel, for
   * non-constant light sources. */
  ShaderDataTinyStorage emission_sd_storage;
  ccl_private ShaderData *emission_sd = AS_SHADER_DATA(&emission_sd_storage);
  const float3 light_eval = light_sample_shader_eval(kg, state, emission_sd, &ls, sd->time);
  if (is_zero(light_eval)) {
    return;
  }

  /* Evaluate BSDF. */
  const bool is_transmission = shader_bsdf_is_transmission(sd, ls.D);

  BsdfEval bsdf_eval ccl_optional_struct_init;
  const float bsdf_pdf = shader_bsdf_eval(kg, sd, ls.D, is_transmission, &bsdf_eval, ls.shader);
  bsdf_eval_mul3(&bsdf_eval, light_eval / ls.pdf);

  if (ls.shader & SHADER_USE_MIS) {
    const float mis_weight = power_heuristic(ls.pdf, bsdf_pdf);
    bsdf_eval_mul(&bsdf_eval, mis_weight);
  }

  /* Path termination. */
  const float terminate = path_state_rng_light_termination(kg, rng_state);
  if (light_sample_terminate(kg, &ls, &bsdf_eval, terminate)) {
    return;
  }

  /* Create shadow ray. */
  Ray ray ccl_optional_struct_init;
  light_sample_to_surface_shadow_ray(kg, sd, &ls, &ray);
  const bool is_light = light_sample_is_light(&ls);

  /* Branch off shadow kernel. */
  INTEGRATOR_SHADOW_PATH_INIT(
      shadow_state, state, DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW, shadow);

  /* Copy volume stack and enter/exit volume. */
  integrator_state_copy_volume_stack_to_shadow(kg, shadow_state, state);

  if (is_transmission) {
#  ifdef __VOLUME__
    shadow_volume_stack_enter_exit(kg, shadow_state, sd);
#  endif
  }

  /* Write shadow ray and associated state to global memory. */
  integrator_state_write_shadow_ray(kg, shadow_state, &ray);

  /* Copy state from main path to shadow path. */
  const uint16_t bounce = INTEGRATOR_STATE(state, path, bounce);
  const uint16_t transparent_bounce = INTEGRATOR_STATE(state, path, transparent_bounce);
  uint32_t shadow_flag = INTEGRATOR_STATE(state, path, flag);
  shadow_flag |= (is_light) ? PATH_RAY_SHADOW_FOR_LIGHT : 0;
  shadow_flag |= (is_transmission) ? PATH_RAY_TRANSMISSION_PASS : PATH_RAY_REFLECT_PASS;
  const float3 throughput = INTEGRATOR_STATE(state, path, throughput) * bsdf_eval_sum(&bsdf_eval);

  if (kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_PASSES) {
    const float3 diffuse_glossy_ratio = (bounce == 0) ?
                                            bsdf_eval_diffuse_glossy_ratio(&bsdf_eval) :
                                            INTEGRATOR_STATE(state, path, diffuse_glossy_ratio);
    INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, diffuse_glossy_ratio) = diffuse_glossy_ratio;
  }

  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, render_pixel_index) = INTEGRATOR_STATE(
      state, path, render_pixel_index);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, rng_offset) = INTEGRATOR_STATE(
      state, path, rng_offset);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, rng_hash) = INTEGRATOR_STATE(
      state, path, rng_hash);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, sample) = INTEGRATOR_STATE(
      state, path, sample);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, flag) = shadow_flag;
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, bounce) = bounce;
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, transparent_bounce) = transparent_bounce;
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, diffuse_bounce) = INTEGRATOR_STATE(
      state, path, diffuse_bounce);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, glossy_bounce) = INTEGRATOR_STATE(
      state, path, glossy_bounce);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, transmission_bounce) = INTEGRATOR_STATE(
      state, path, transmission_bounce);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, throughput) = throughput;

  if (kernel_data.kernel_features & KERNEL_FEATURE_SHADOW_PASS) {
    INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, unshadowed_throughput) = throughput;
  }
}
#endif

/* Path tracing: bounce off or through surface with new direction. */
ccl_device_forceinline int integrate_surface_bsdf_bssrdf_bounce(
    KernelGlobals kg,
    IntegratorState state,
    ccl_private ShaderData *sd,
    ccl_private const RNGState *rng_state)
{
  /* Sample BSDF or BSSRDF. */
  if (!(sd->flag & (SD_BSDF | SD_BSSRDF))) {
    return LABEL_NONE;
  }

  float bsdf_u, bsdf_v;
  path_state_rng_2D(kg, rng_state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);
  ccl_private const ShaderClosure *sc = shader_bsdf_bssrdf_pick(sd, &bsdf_u);

#ifdef __SUBSURFACE__
  /* BSSRDF closure, we schedule subsurface intersection kernel. */
  if (CLOSURE_IS_BSSRDF(sc->type)) {
    return subsurface_bounce(kg, state, sd, sc);
  }
#endif

  /* BSDF closure, sample direction. */
  float bsdf_pdf;
  BsdfEval bsdf_eval ccl_optional_struct_init;
  float3 bsdf_omega_in ccl_optional_struct_init;
  differential3 bsdf_domega_in ccl_optional_struct_init;
  int label;

  label = shader_bsdf_sample_closure(
      kg, sd, sc, bsdf_u, bsdf_v, &bsdf_eval, &bsdf_omega_in, &bsdf_domega_in, &bsdf_pdf);

  if (bsdf_pdf == 0.0f || bsdf_eval_is_zero(&bsdf_eval)) {
    return LABEL_NONE;
  }

  /* Setup ray. Note that clipping works through transparent bounces. */
  INTEGRATOR_STATE_WRITE(state, ray, P) = ray_offset(sd->P,
                                                     (label & LABEL_TRANSMIT) ? -sd->Ng : sd->Ng);
  INTEGRATOR_STATE_WRITE(state, ray, D) = normalize(bsdf_omega_in);
  INTEGRATOR_STATE_WRITE(state, ray, t) = (label & LABEL_TRANSPARENT) ?
                                              INTEGRATOR_STATE(state, ray, t) - sd->ray_length :
                                              FLT_MAX;

#ifdef __RAY_DIFFERENTIALS__
  INTEGRATOR_STATE_WRITE(state, ray, dP) = differential_make_compact(sd->dP);
  INTEGRATOR_STATE_WRITE(state, ray, dD) = differential_make_compact(bsdf_domega_in);
#endif

  /* Update throughput. */
  float3 throughput = INTEGRATOR_STATE(state, path, throughput);
  throughput *= bsdf_eval_sum(&bsdf_eval) / bsdf_pdf;
  INTEGRATOR_STATE_WRITE(state, path, throughput) = throughput;

  if (kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_PASSES) {
    if (INTEGRATOR_STATE(state, path, bounce) == 0) {
      INTEGRATOR_STATE_WRITE(state, path, diffuse_glossy_ratio) = bsdf_eval_diffuse_glossy_ratio(
          &bsdf_eval);
    }
  }

  /* Update path state */
  if (label & LABEL_TRANSPARENT) {
    INTEGRATOR_STATE_WRITE(state, path, mis_ray_t) += sd->ray_length;
  }
  else {
    INTEGRATOR_STATE_WRITE(state, path, mis_ray_pdf) = bsdf_pdf;
    INTEGRATOR_STATE_WRITE(state, path, mis_ray_t) = 0.0f;
    INTEGRATOR_STATE_WRITE(state, path, min_ray_pdf) = fminf(
        bsdf_pdf, INTEGRATOR_STATE(state, path, min_ray_pdf));
  }

  path_state_next(kg, state, label);
  return label;
}

#ifdef __VOLUME__
ccl_device_forceinline bool integrate_surface_volume_only_bounce(IntegratorState state,
                                                                 ccl_private ShaderData *sd)
{
  if (!path_state_volume_next(state)) {
    return LABEL_NONE;
  }

  /* Setup ray position, direction stays unchanged. */
  INTEGRATOR_STATE_WRITE(state, ray, P) = ray_offset(sd->P, -sd->Ng);

  /* Clipping works through transparent. */
  INTEGRATOR_STATE_WRITE(state, ray, t) -= sd->ray_length;

#  ifdef __RAY_DIFFERENTIALS__
  INTEGRATOR_STATE_WRITE(state, ray, dP) = differential_make_compact(sd->dP);
#  endif

  INTEGRATOR_STATE_WRITE(state, path, mis_ray_t) += sd->ray_length;

  return LABEL_TRANSMIT | LABEL_TRANSPARENT;
}
#endif

#if defined(__AO__)
ccl_device_forceinline void integrate_surface_ao(KernelGlobals kg,
                                                 IntegratorState state,
                                                 ccl_private const ShaderData *ccl_restrict sd,
                                                 ccl_private const RNGState *ccl_restrict
                                                     rng_state,
                                                 ccl_global float *ccl_restrict render_buffer)
{
  if (!(kernel_data.kernel_features & KERNEL_FEATURE_AO_ADDITIVE) &&
      !(INTEGRATOR_STATE(state, path, flag) & PATH_RAY_CAMERA)) {
    return;
  }

  float bsdf_u, bsdf_v;
  path_state_rng_2D(kg, rng_state, PRNG_BSDF_U, &bsdf_u, &bsdf_v);

  float3 ao_N;
  const float3 ao_weight = shader_bsdf_ao(
      kg, sd, kernel_data.integrator.ao_additive_factor, &ao_N);

  float3 ao_D;
  float ao_pdf;
  sample_cos_hemisphere(ao_N, bsdf_u, bsdf_v, &ao_D, &ao_pdf);

  if (!(dot(sd->Ng, ao_D) > 0.0f && ao_pdf != 0.0f)) {
    return;
  }

  Ray ray ccl_optional_struct_init;
  ray.P = ray_offset(sd->P, sd->Ng);
  ray.D = ao_D;
  ray.t = kernel_data.integrator.ao_bounces_distance;
  ray.time = sd->time;
  ray.dP = differential_zero_compact();
  ray.dD = differential_zero_compact();

  /* Branch off shadow kernel. */
  INTEGRATOR_SHADOW_PATH_INIT(shadow_state, state, DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW, ao);

  /* Copy volume stack and enter/exit volume. */
  integrator_state_copy_volume_stack_to_shadow(kg, shadow_state, state);

  /* Write shadow ray and associated state to global memory. */
  integrator_state_write_shadow_ray(kg, shadow_state, &ray);

  /* Copy state from main path to shadow path. */
  const uint16_t bounce = INTEGRATOR_STATE(state, path, bounce);
  const uint16_t transparent_bounce = INTEGRATOR_STATE(state, path, transparent_bounce);
  uint32_t shadow_flag = INTEGRATOR_STATE(state, path, flag) | PATH_RAY_SHADOW_FOR_AO;
  const float3 throughput = INTEGRATOR_STATE(state, path, throughput) * shader_bsdf_alpha(kg, sd);

  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, render_pixel_index) = INTEGRATOR_STATE(
      state, path, render_pixel_index);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, rng_offset) = INTEGRATOR_STATE(
      state, path, rng_offset);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, rng_hash) = INTEGRATOR_STATE(
      state, path, rng_hash);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, sample) = INTEGRATOR_STATE(
      state, path, sample);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, flag) = shadow_flag;
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, bounce) = bounce;
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, transparent_bounce) = transparent_bounce;
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, throughput) = throughput;

  if (kernel_data.kernel_features & KERNEL_FEATURE_AO_ADDITIVE) {
    INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, unshadowed_throughput) = ao_weight;
  }
}
#endif /* defined(__AO__) */

template<uint node_feature_mask>
ccl_device bool integrate_surface(KernelGlobals kg,
                                  IntegratorState state,
                                  ccl_global float *ccl_restrict render_buffer)

{
  PROFILING_INIT_FOR_SHADER(kg, PROFILING_SHADE_SURFACE_SETUP);

  /* Setup shader data. */
  ShaderData sd;
  integrate_surface_shader_setup(kg, state, &sd);
  PROFILING_SHADER(sd.object, sd.shader);

  int continue_path_label = 0;

  /* Skip most work for volume bounding surface. */
#ifdef __VOLUME__
  if (!(sd.flag & SD_HAS_ONLY_VOLUME)) {
#endif
    const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

#ifdef __SUBSURFACE__
    /* Can skip shader evaluation for BSSRDF exit point without bump mapping. */
    if (!(path_flag & PATH_RAY_SUBSURFACE) || ((sd.flag & SD_HAS_BSSRDF_BUMP)))
#endif
    {
      /* Evaluate shader. */
      PROFILING_EVENT(PROFILING_SHADE_SURFACE_EVAL);
      shader_eval_surface<node_feature_mask>(kg, state, &sd, render_buffer, path_flag);

      /* Initialize additional RNG for BSDFs. */
      if (sd.flag & SD_BSDF_NEEDS_LCG) {
        sd.lcg_state = lcg_state_init(INTEGRATOR_STATE(state, path, rng_hash),
                                      INTEGRATOR_STATE(state, path, rng_offset),
                                      INTEGRATOR_STATE(state, path, sample),
                                      0xb4bc3953);
      }
    }

#ifdef __SUBSURFACE__
    if (path_flag & PATH_RAY_SUBSURFACE) {
      /* When coming from inside subsurface scattering, setup a diffuse
       * closure to perform lighting at the exit point. */
      subsurface_shader_data_setup(kg, state, &sd, path_flag);
      INTEGRATOR_STATE_WRITE(state, path, flag) &= ~PATH_RAY_SUBSURFACE;
    }
#endif

    shader_prepare_surface_closures(kg, state, &sd);

#ifdef __HOLDOUT__
    /* Evaluate holdout. */
    if (!integrate_surface_holdout(kg, state, &sd, render_buffer)) {
      return false;
    }
#endif

#ifdef __EMISSION__
    /* Write emission. */
    if (sd.flag & SD_EMISSION) {
      integrate_surface_emission(kg, state, &sd, render_buffer);
    }
#endif

#ifdef __PASSES__
    /* Write render passes. */
    PROFILING_EVENT(PROFILING_SHADE_SURFACE_PASSES);
    kernel_write_data_passes(kg, state, &sd, render_buffer);
#endif

    /* Load random number state. */
    RNGState rng_state;
    path_state_rng_load(state, &rng_state);

    /* Perform path termination. Most paths have already been terminated in
     * the intersect_closest kernel, this is just for emission and for dividing
     * throughput by the probability at the right moment.
     *
     * Also ensure we don't do it twice for SSS at both the entry and exit point. */
    if (!(path_flag & PATH_RAY_SUBSURFACE)) {
      const float probability = (path_flag & PATH_RAY_TERMINATE_ON_NEXT_SURFACE) ?
                                    0.0f :
                                    INTEGRATOR_STATE(state, path, continuation_probability);
      if (probability == 0.0f) {
        return false;
      }
      else if (probability != 1.0f) {
        INTEGRATOR_STATE_WRITE(state, path, throughput) /= probability;
      }
    }

#ifdef __DENOISING_FEATURES__
    kernel_write_denoising_features_surface(kg, state, &sd, render_buffer);
#endif

#ifdef __SHADOW_CATCHER__
    kernel_write_shadow_catcher_bounce_data(kg, state, &sd, render_buffer);
#endif

    /* Direct light. */
    PROFILING_EVENT(PROFILING_SHADE_SURFACE_DIRECT_LIGHT);
    integrate_surface_direct_light(kg, state, &sd, &rng_state);

#if defined(__AO__)
    /* Ambient occlusion pass. */
    if (kernel_data.kernel_features & KERNEL_FEATURE_AO) {
      PROFILING_EVENT(PROFILING_SHADE_SURFACE_AO);
      integrate_surface_ao(kg, state, &sd, &rng_state, render_buffer);
    }
#endif

    PROFILING_EVENT(PROFILING_SHADE_SURFACE_INDIRECT_LIGHT);
    continue_path_label = integrate_surface_bsdf_bssrdf_bounce(kg, state, &sd, &rng_state);
#ifdef __VOLUME__
  }
  else {
    PROFILING_EVENT(PROFILING_SHADE_SURFACE_INDIRECT_LIGHT);
    continue_path_label = integrate_surface_volume_only_bounce(state, &sd);
  }

  if (continue_path_label & LABEL_TRANSMIT) {
    /* Enter/Exit volume. */
    volume_stack_enter_exit(kg, state, &sd);
  }
#endif

  return continue_path_label != 0;
}

template<uint node_feature_mask = KERNEL_FEATURE_NODE_MASK_SURFACE & ~KERNEL_FEATURE_NODE_RAYTRACE,
         int current_kernel = DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE>
ccl_device_forceinline void integrator_shade_surface(KernelGlobals kg,
                                                     IntegratorState state,
                                                     ccl_global float *ccl_restrict render_buffer)
{
  if (integrate_surface<node_feature_mask>(kg, state, render_buffer)) {
    if (INTEGRATOR_STATE(state, path, flag) & PATH_RAY_SUBSURFACE) {
      INTEGRATOR_PATH_NEXT(current_kernel, DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE);
    }
    else {
      kernel_assert(INTEGRATOR_STATE(state, ray, t) != 0.0f);
      INTEGRATOR_PATH_NEXT(current_kernel, DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);
    }
  }
  else {
    INTEGRATOR_PATH_TERMINATE(current_kernel);
  }
}

ccl_device_forceinline void integrator_shade_surface_raytrace(
    KernelGlobals kg, IntegratorState state, ccl_global float *ccl_restrict render_buffer)
{
  integrator_shade_surface<KERNEL_FEATURE_NODE_MASK_SURFACE,
                           DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE>(
      kg, state, render_buffer);
}

CCL_NAMESPACE_END
