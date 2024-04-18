/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/film/data_passes.h"
#include "kernel/film/light_passes.h"

#include "kernel/integrator/guiding.h"
#include "kernel/integrator/surface_shader.h"

#include "kernel/light/light.h"
#include "kernel/light/sample.h"

CCL_NAMESPACE_BEGIN

ccl_device Spectrum integrator_eval_background_shader(KernelGlobals kg,
                                                      IntegratorState state,
                                                      ccl_global float *ccl_restrict render_buffer)
{
  const int shader = kernel_data.background.surface_shader;
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  /* Use visibility flag to skip lights. */
  if (!is_light_shader_visible_to_path(shader, path_flag)) {
    return zero_spectrum();
  }

  /* Use fast constant background color if available. */
  Spectrum L = zero_spectrum();
  if (surface_shader_constant_emission(kg, shader, &L)) {
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
                               INTEGRATOR_STATE(state, ray, time));

  PROFILING_SHADER(emission_sd->object, emission_sd->shader);
  PROFILING_EVENT(PROFILING_SHADE_LIGHT_EVAL);
  surface_shader_eval<KERNEL_FEATURE_NODE_MASK_SURFACE_BACKGROUND>(
      kg, state, emission_sd, render_buffer, path_flag | PATH_RAY_EMISSION);

  return surface_shader_background(emission_sd);
}

ccl_device_inline void integrate_background(KernelGlobals kg,
                                            IntegratorState state,
                                            ccl_global float *ccl_restrict render_buffer)
{
  /* Accumulate transparency for transparent background. We can skip background
   * shader evaluation unless a background pass is used. */
  bool eval_background = true;
  float transparent = 0.0f;

  int path_flag = INTEGRATOR_STATE(state, path, flag);
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
    L = integrator_eval_background_shader(kg, state, render_buffer);

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
}

ccl_device_inline void integrate_distant_lights(KernelGlobals kg,
                                                IntegratorState state,
                                                ccl_global float *ccl_restrict render_buffer)
{
  const float3 ray_D = INTEGRATOR_STATE(state, ray, D);
  const float ray_time = INTEGRATOR_STATE(state, ray, time);
  LightSample ls ccl_optional_struct_init;
  for (int lamp = 0; lamp < kernel_data.integrator.num_lights; lamp++) {
    if (distant_light_sample_from_intersection(kg, ray_D, lamp, &ls)) {
      /* Use visibility flag to skip lights. */
#ifdef __PASSES__
      const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
      if (!is_light_shader_visible_to_path(ls.shader, path_flag)) {
        continue;
      }
#endif

#ifdef __LIGHT_LINKING__
      if (!light_link_light_match(kg, light_link_receiver_forward(kg, state), lamp) &&
          !(path_flag & PATH_RAY_CAMERA))
      {
        continue;
      }
#endif
#ifdef __SHADOW_LINKING__
      if (kernel_data_fetch(lights, lamp).shadow_set_membership != LIGHT_LINK_MASK_ALL) {
        continue;
      }
#endif

#ifdef __MNEE__
      if (INTEGRATOR_STATE(state, path, mnee) & PATH_MNEE_CULL_LIGHT_CONNECTION) {
        /* This path should have been resolved with mnee, it will
         * generate a firefly for small lights since it is improbable. */
        const ccl_global KernelLight *klight = &kernel_data_fetch(lights, lamp);
        if (klight->use_caustics)
          continue;
      }
#endif /* __MNEE__ */

      /* Evaluate light shader. */
      /* TODO: does aliasing like this break automatic SoA in CUDA? */
      ShaderDataTinyStorage emission_sd_storage;
      ccl_private ShaderData *emission_sd = AS_SHADER_DATA(&emission_sd_storage);
      Spectrum light_eval = light_sample_shader_eval(kg, state, emission_sd, &ls, ray_time);
      if (is_zero(light_eval)) {
        continue;
      }

      /* MIS weighting. */
      const float mis_weight = light_sample_mis_weight_forward_distant(kg, state, path_flag, &ls);

      /* Write to render buffer. */
      guiding_record_background(kg, state, light_eval, mis_weight);
      film_write_surface_emission(kg, state, light_eval, mis_weight, render_buffer, ls.group);
    }
  }
}

ccl_device void integrator_shade_background(KernelGlobals kg,
                                            IntegratorState state,
                                            ccl_global float *ccl_restrict render_buffer)
{
  PROFILING_INIT(kg, PROFILING_SHADE_LIGHT_SETUP);

  /* TODO: unify these in a single loop to only have a single shader evaluation call. */
  integrate_distant_lights(kg, state, render_buffer);
  integrate_background(kg, state, render_buffer);

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

  integrator_path_terminate(kg, state, DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND);
}

CCL_NAMESPACE_END
