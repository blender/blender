/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/film/data_passes.h"
#include "kernel/film/light_passes.h"

#include "kernel/integrator/guiding.h"
#include "kernel/integrator/intersect_closest.h"
#include "kernel/integrator/state_flow.h"
#include "kernel/integrator/surface_shader.h"

#include "kernel/light/light.h"
#include "kernel/light/sample.h"

#include "kernel/geom/object.h"
#include "kernel/geom/shader_data.h"

#include "kernel/types.h"

CCL_NAMESPACE_BEGIN

ccl_device void integrator_shade_background_cache_miss_set_resume_offset(IntegratorState state,
                                                                         const int resume_offset)
{
  /* Abuse prim field that is not used by shade_background. */
  INTEGRATOR_STATE_WRITE(state, isect, prim) = resume_offset;
}

ccl_device int integrator_shade_background_cache_miss_get_resume_offset(IntegratorState state)
{
  /* Abuse prim field that is not used by shade_background. */
  const int resume_offset = INTEGRATOR_STATE_WRITE(state, isect, prim);
  return (resume_offset == PRIM_NONE) ? 0 : resume_offset;
}

ccl_device Spectrum integrator_eval_background_shader(KernelGlobals kg,
                                                      IntegratorState state,
                                                      ccl_global float *ccl_restrict render_buffer,
                                                      ccl_private ShaderEvalResult &result)
{
  const int shader = kernel_data.background.surface_shader;
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  /* Use visibility flag to skip lights. */
  if (!is_light_shader_visible_to_path(shader, path_flag)) {
    result = SHADER_EVAL_EMPTY;
    return zero_spectrum();
  }

  /* Use fast constant background color if available. */
  Spectrum L = zero_spectrum();
  if (surface_shader_constant_emission(kg, shader, &L)) {
    result = SHADER_EVAL_OK;
    return L;
  }

  /* Evaluate background shader. */

  /* TODO: does aliasing like this break automatic SoA in CUDA?
   * Should we instead store closures separate from ShaderData? */
  ShaderDataTinyStorage emission_sd_storage;
  ccl_private ShaderData *emission_sd = AS_SHADER_DATA(&emission_sd_storage);

  PROFILING_INIT_FOR_SHADER(kg, PROFILING_SHADE_LIGHT_SETUP);
  shader_setup_from_background(kg,
                               emission_sd,
                               INTEGRATOR_STATE(state, ray, P),
                               INTEGRATOR_STATE(state, ray, D),
                               INTEGRATOR_STATE(state, ray, dD),
                               INTEGRATOR_STATE(state, ray, time));

  PROFILING_SHADER(emission_sd->object, emission_sd->shader);
  PROFILING_EVENT(PROFILING_SHADE_LIGHT_EVAL);
  surface_shader_eval<KERNEL_FEATURE_NODE_MASK_SURFACE_BACKGROUND>(
      kg, state, emission_sd, render_buffer, path_flag | PATH_RAY_EMISSION);

  result = (emission_sd->flag & SD_CACHE_MISS) ? SHADER_EVAL_CACHE_MISS : SHADER_EVAL_OK;
  return surface_shader_background(emission_sd);
}

ccl_device_inline ShaderEvalResult integrate_background(
    KernelGlobals kg, IntegratorState state, ccl_global float *ccl_restrict render_buffer)
{
  /* Accumulate transparency for transparent background. We can skip background
   * shader evaluation unless a background pass is used. */
  bool eval_background = true;
  float transparent = 0.0f;

  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  const bool is_transparent_background_ray = kernel_data.background.transparent &&
                                             (path_flag & PATH_RAY_TRANSPARENT_BACKGROUND);

  if (is_transparent_background_ray) {
    transparent = average(INTEGRATOR_STATE(state, path, throughput));

#ifdef __PASSES__
    eval_background = (kernel_data.film.light_pass_flag & PASSMASK(BACKGROUND));
#else
    eval_background = false;
#endif
  }

#ifdef __MNEE__
  if (INTEGRATOR_STATE(state, path, mnee) & PATH_MNEE_CULL_LIGHT_CONNECTION) {
    if (kernel_data.background.use_mis) {
      for (int lamp = 0; lamp < kernel_data.integrator.num_lights; lamp++) {
        /* This path should have been resolved with mnee, it will
         * generate a firefly for small lights since it is improbable. */
        const ccl_global KernelLight *klight = &kernel_data_fetch(lights, lamp);
        if (klight->type == LIGHT_BACKGROUND && klight->use_caustics) {
          eval_background = false;
          break;
        }
      }
    }
  }
#endif /* __MNEE__ */

  /* Evaluate background shader. */
  Spectrum L = zero_spectrum();

  if (eval_background) {
    ShaderEvalResult result = SHADER_EVAL_EMPTY;
    L = integrator_eval_background_shader(kg, state, render_buffer, result);
    if (result == SHADER_EVAL_CACHE_MISS) {
      integrator_shade_background_cache_miss_set_resume_offset(state,
                                                               kernel_data.integrator.num_lights);
      return SHADER_EVAL_CACHE_MISS;
    }

    /* When using the ao bounces approximation, adjust background
     * shader intensity with ao factor. */
    if (path_state_ao_bounce(kg, state)) {
      L *= kernel_data.integrator.ao_bounces_factor;
    }

    /* Background MIS weights. */
    const float mis_weight = light_sample_mis_weight_forward_background(kg, state, path_flag);

    guiding_record_background(kg, state, L, mis_weight);
    L *= mis_weight;
  }

  /* Write to render buffer. */
  film_write_background(kg, state, L, transparent, is_transparent_background_ray, render_buffer);
  film_write_data_passes_background(kg, state, render_buffer);

  return SHADER_EVAL_OK;
}

ccl_device_inline ShaderEvalResult integrate_distant_lights(
    KernelGlobals kg, IntegratorState state, ccl_global float *ccl_restrict render_buffer)
{
  const float3 ray_D = INTEGRATOR_STATE(state, ray, D);
  const float ray_time = INTEGRATOR_STATE(state, ray, time);
  const int lamp_offset = integrator_shade_background_cache_miss_get_resume_offset(state);

  for (int lamp = lamp_offset; lamp < kernel_data.integrator.num_lights; lamp++) {
    const ccl_global KernelLight *klight = &kernel_data_fetch(lights, lamp);

    if (klight->type != LIGHT_DISTANT || !(klight->shader_id & SHADER_USE_MIS)) {
      continue;
    }

    LightEval light_eval = distant_light_eval_from_intersection(klight, ray_D);
    if (light_eval.eval_fac == 0.0f) {
      continue;
    }

    /* Use visibility flag to skip lights. */
#ifdef __PASSES__
    const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
    if (!is_light_shader_visible_to_path(klight->shader_id, path_flag)) {
      continue;
    }
#endif

#ifdef __LIGHT_LINKING__
    if (!light_link_light_match(kg, light_link_receiver_forward(kg, state), klight->object_id) &&
        !(path_flag & PATH_RAY_CAMERA))
    {
      continue;
    }
#endif
#ifdef __SHADOW_LINKING__
    if (kernel_data_fetch(objects, klight->object_id).shadow_set_membership != LIGHT_LINK_MASK_ALL)
    {
      continue;
    }
#endif

#ifdef __MNEE__
    if (INTEGRATOR_STATE(state, path, mnee) & PATH_MNEE_CULL_LIGHT_CONNECTION) {
      /* This path should have been resolved with mnee, it will
       * generate a firefly for small lights since it is improbable. */
      if (klight->use_caustics) {
        continue;
      }
    }
#endif /* __MNEE__ */

    /* Evaluate light shader. */
    Spectrum shader_eval;
    const ShaderEvalResult eval_result = light_sample_shader_eval_forward(
        kg, state, lamp, zero_float3(), ray_D, FLT_MAX, ray_time, shader_eval);

    if (eval_result == SHADER_EVAL_CACHE_MISS) {
      integrator_shade_background_cache_miss_set_resume_offset(state, lamp);
      return SHADER_EVAL_CACHE_MISS;
    }

    const float3 eval = shader_eval * light_eval.eval_fac;
    if (is_zero(eval)) {
      continue;
    }

    /* MIS weighting. */
    const float mis_weight = light_sample_mis_weight_forward_distant(
        kg, state, path_flag, lamp, light_eval.pdf);

    /* Write to render buffer. */
    guiding_record_background(kg, state, eval, mis_weight);
    film_write_surface_emission(
        kg, state, eval, mis_weight, render_buffer, object_lightgroup(kg, klight->object_id));
  }

  return SHADER_EVAL_OK;
}

ccl_device void integrator_shade_background(KernelGlobals kg,
                                            IntegratorState state,
                                            ccl_global float *ccl_restrict render_buffer)
{
  PROFILING_INIT(kg, PROFILING_SHADE_LIGHT_SETUP);

  /* TODO: unify these in a single loop to only have a single shader evaluation call. */
  ShaderEvalResult result = integrate_distant_lights(kg, state, render_buffer);
  if (result == SHADER_EVAL_CACHE_MISS) {
    integrator_path_cache_miss(state, DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND);
    return;
  }
  result = integrate_background(kg, state, render_buffer);
  if (result == SHADER_EVAL_CACHE_MISS) {
    integrator_path_cache_miss(state, DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND);
    return;
  }

#ifdef __SHADOW_CATCHER__
  if (INTEGRATOR_STATE(state, path, flag) & PATH_RAY_SHADOW_CATCHER_BACKGROUND) {
    /* Special case for shadow catcher where we want to fill the background pass
     * behind the shadow catcher but also continue tracing the path. */
    INTEGRATOR_STATE_WRITE(state, path, flag) &= ~PATH_RAY_SHADOW_CATCHER_BACKGROUND;
    integrator_intersect_next_kernel_after_shadow_catcher_background<
        DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND>(kg, state);
    return;
  }
#endif

  integrator_path_terminate(kg, state, render_buffer, DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND);
}

CCL_NAMESPACE_END
