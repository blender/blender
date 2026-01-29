/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/film/light_passes.h"

#include "kernel/integrator/path_state.h"

#include "kernel/light/light.h"
#include "kernel/light/sample.h"

#include "kernel/geom/object.h"
#include "kernel/types.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline void integrate_light_forward(KernelGlobals kg,
                                               IntegratorState state,
                                               ccl_global float *ccl_restrict render_buffer)
{
  /* Setup light sample. */
  Intersection isect ccl_optional_struct_init;
  integrator_state_read_isect(state, &isect);

  guiding_record_light_surface_segment(kg, state, &isect);

  const float3 ray_P = INTEGRATOR_STATE(state, ray, P);
  const float3 ray_D = INTEGRATOR_STATE(state, ray, D);
  const float ray_time = INTEGRATOR_STATE(state, ray, time);
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  const float3 N = INTEGRATOR_STATE(state, path, mis_origin_n);

  /* Advance ray to new start distance. */
  INTEGRATOR_STATE_WRITE(state, ray, tmin) = intersection_t_offset(isect.t);

  const LightEval light_eval = light_eval_from_intersection(
      kg, &isect, ray_P, ray_D, N, path_flag);
  if (light_eval.eval_fac == 0.0f) {
    return;
  }

  /* Use visibility flag to skip lights. */
#ifdef __PASSES__
  {
    const ccl_global KernelLight *klight = &kernel_data_fetch(lights, isect.prim);
    if (!is_light_shader_visible_to_path(klight->shader_id, path_flag)) {
      return;
    }
  }
#endif

  /* Evaluate light shader. */
  const Spectrum shader_eval = light_sample_shader_eval_forward(
      kg, state, isect.prim, ray_P, ray_D, isect.t, ray_time);
  const float3 eval = shader_eval * light_eval.eval_fac;
  if (is_zero(eval)) {
    return;
  }

  /* MIS weighting. */
  const float mis_weight = light_sample_mis_weight_forward_lamp(
      kg, state, path_flag, isect.prim, light_eval.pdf, ray_P);

  /* Write to render buffer. */
  guiding_record_surface_emission(kg, state, eval, mis_weight);
  const ccl_global KernelLight *klight = &kernel_data_fetch(lights, isect.prim);
  film_write_surface_emission(
      kg, state, eval, mis_weight, render_buffer, object_lightgroup(kg, klight->object_id));
}

/* Evaluate light shader at intersection in forward path tracing. */
ccl_device void integrator_shade_light_forward(KernelGlobals kg,
                                               IntegratorState state,
                                               ccl_global float *ccl_restrict render_buffer)
{
  PROFILING_INIT(kg, PROFILING_SHADE_LIGHT_SETUP);

  integrate_light_forward(kg, state, render_buffer);

  /* TODO: we could get stuck in an infinite loop if there are precision issues
   * and the same light is hit again.
   *
   * As a workaround count this as a transparent bounce. It makes some sense
   * to interpret lights as transparent surfaces (and support making them opaque),
   * but this needs to be revisited. */
  const int transparent_bounce = INTEGRATOR_STATE(state, path, transparent_bounce) + 1;
  INTEGRATOR_STATE_WRITE(state, path, transparent_bounce) = transparent_bounce;

  if (transparent_bounce >= kernel_data.integrator.transparent_max_bounce) {
    integrator_path_terminate(
        kg, state, render_buffer, DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT_FORWARD);
    return;
  }

  integrator_path_next(state,
                       DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT_FORWARD,
                       DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);

  /* TODO: in some cases we could continue directly to SHADE_BACKGROUND, but
   * probably that optimization is probably not practical if we add lights to
   * scene geometry. */
}

ccl_device bool integrate_light_nee(KernelGlobals kg, IntegratorShadowState state)
{
  /* Read intersection and ray. */
  Ray ray ccl_optional_struct_init;
  integrator_state_read_shadow_ray(state, &ray);
  integrator_state_read_shadow_ray_self(state, &ray);

  Intersection isect = {};
  isect.object = ray.self.light_object;
  isect.prim = ray.self.light_prim;
  isect.type = kernel_data_fetch(objects, isect.object).primitive_type;
  isect.t = ray.tmax;

  kernel_assert(isect.object != OBJECT_NONE);
  kernel_assert(isect.prim != PRIM_NONE);

  float3 eval = zero_spectrum();
  bool is_background = false;

  /* Setup shader data */
  ShaderDataCausticsStorage emission_sd_storage;
  ccl_private ShaderData *emission_sd = AS_SHADER_DATA(&emission_sd_storage);

  PROFILING_INIT_FOR_SHADER(kg, PROFILING_SHADE_LIGHT_SETUP);
  if (isect.type == PRIMITIVE_LAMP) {
    /* Lights. */
    const ccl_global KernelLight *klight = &kernel_data_fetch(lights, isect.prim);
    const LightType light_type = LightType(klight->type);

    if (light_type == LIGHT_BACKGROUND) {
      /* Background light. */
      shader_setup_from_background(kg, emission_sd, ray.P, ray.D, ray.time);
      is_background = true;
    }
    else {
      /* Other light types.
       * Compute Ng and UV on demand so we don't have to store it in integrator state. */
      const float3 P = (ray.tmax == FLT_MAX) ? -ray.D : ray.P + ray.tmax * ray.D;
      float3 Ng = zero_float3();
      float2 uv = zero_float2();
      light_normal_uv_from_position(kg, klight, P, ray.D, Ng, uv);

      shader_setup_from_sample(kg,
                               emission_sd,
                               P,
                               Ng,
                               -ray.D,
                               klight->shader_id,
                               isect.object,
                               isect.prim,
                               uv.x,
                               uv.y,
                               ray.tmax,
                               ray.time,
                               false,
                               true);
    }
  }
  else {
    /* Triangles.
     * Compute UV on demand so we don't have to store it in integrator state. */
    const float2 uv = triangle_light_uv(kg, isect.object, isect.prim, ray.time, ray.P, ray.D);
    isect.u = uv.x;
    isect.v = uv.y;

    shader_setup_from_ray(kg, emission_sd, &ray, &isect);
  }

  /* Evaluate shader. */
  PROFILING_SHADER(emission_sd->object, emission_sd->shader);
  PROFILING_EVENT(PROFILING_SHADE_LIGHT_EVAL);

  /* No proper path flag, we're evaluating this for all closures. that's
   * weak but we'd have to do multiple evaluations otherwise. */
  surface_shader_eval<KERNEL_FEATURE_NODE_MASK_SURFACE_LIGHT>(
      kg, state, emission_sd, nullptr, PATH_RAY_EMISSION);

  /* Evaluate emission closures. */
  eval = (is_background) ? surface_shader_background(emission_sd) :
                           surface_shader_emission(emission_sd);

  /* Probabilistic light termination.
   * Light threshold is only used without light tree. */
  if (!(kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_TREE)) {
    RNGState rng_state;
    shadow_path_state_rng_load(state, &rng_state);
    const float rand_terminate = path_state_rng_light_termination(kg, &rng_state);
    const float bsdf_eval_average = INTEGRATOR_STATE(state, shadow_path, bsdf_eval_average);
    if (light_sample_terminate(kg, eval, bsdf_eval_average, rand_terminate)) {
      return false;
    }
  }
  else if (is_zero(eval)) {
    return false;
  }

  /* Update throughput. */
  INTEGRATOR_STATE(state, shadow_path, throughput) *= eval;

  return true;
}

/* Evaluate light shader for next event estimation, after shade_surface and shade_volume and before
 * shadow ray intersection. Only when the light has non-constant emission. */
ccl_device void integrator_shade_light_nee(KernelGlobals kg,
                                           IntegratorShadowState state,
                                           ccl_global float *ccl_restrict /*render_buffer*/)
{
  PROFILING_INIT(kg, PROFILING_SHADE_LIGHT_SETUP);

  if (!integrate_light_nee(kg, state)) {
    integrator_shadow_path_terminate(state, DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT_NEE);
    return;
  }

  integrator_shadow_path_next(
      state, DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT_NEE, DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW);
}

CCL_NAMESPACE_END
