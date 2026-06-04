/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/integrator/mnee.h"
#include "kernel/integrator/shade_surface.h"

CCL_NAMESPACE_BEGIN

#ifdef __MNEE__

/* Sample a light and run the MNEE manifold walk for a caustic receiver. On a successful walk
 * the result is written to a shadow slot for integrator_shade_surface, otherwise nothing is
 * written and direct light is sampled there. */
ccl_device_forceinline ShaderEvalResult
integrate_surface_mnee(KernelGlobals kg,
                       IntegratorState state,
                       ccl_private ShaderData *sd,
                       const ccl_private RNGState *rng_state)
{
  /* Kernel must only be scheduled for caustic receivers. */
  kernel_assert(sd->object_flag & SD_OBJECT_CAUSTICS_RECEIVER);

  if (!kernel_data.integrator.use_direct_light) {
    return SHADER_EVAL_OK;
  }

  /* Sample position on a light. */
  LightSample ls ccl_optional_struct_init;
  {
    const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
    const uint bounce = INTEGRATOR_STATE(state, path, bounce);
    const float3 rand_light = path_state_rng_3D(kg, rng_state, PRNG_LIGHT);

    if (!light_sample_from_position(kg,
                                    rand_light,
                                    sd->time,
                                    sd->P,
                                    sd->N,
                                    light_link_receiver_nee(kg, sd),
                                    sd->flag,
                                    bounce,
                                    path_flag,
                                    &ls))
    {
      return SHADER_EVAL_OK;
    }
  }

  kernel_assert(ls.pdf != 0.0f);

  /* The manifold walk connects a caustic light to the receiver across reflection; transmission
   * caustics and triangle lights are not handled. */
  if (ls.type == LIGHT_TRIANGLE || dot(ls.D, sd->N) < 0.0f) {
    return SHADER_EVAL_OK;
  }

  if (!kernel_data_fetch(lights, ls.prim).use_caustics) {
    return SHADER_EVAL_OK;
  }

  ShaderDataCausticsStorage emission_sd_storage;
  ccl_private ShaderData *emission_sd = AS_SHADER_DATA(&emission_sd_storage);

  Spectrum mnee_throughput = zero_spectrum();
  float3 mnee_wo = zero_float3();
  int mnee_vertex_count = 0;
  const ShaderEvalResult result = kernel_path_mnee_sample(
      kg, state, sd, emission_sd, rng_state, &ls, &mnee_throughput, &mnee_wo, mnee_vertex_count);
  if (result == SHADER_EVAL_CACHE_MISS) {
    return SHADER_EVAL_CACHE_MISS;
  }

  /* Store MNEE state in a shadow state, to avoid increasing path state size.
   * This is then turned into an actual shadow ray state in shade_surface, or discarded. */
  if (mnee_vertex_count > 0) {
    Ray ray ccl_optional_struct_init;
    light_sample_to_surface_shadow_ray(kg, emission_sd, &ls, &ray);

    IntegratorShadowState shadow_state = integrator_shadow_path_init(
        kg, state, DEVICE_KERNEL_INTEGRATOR_SHADOW_PATH_MNEE_PENDING, false);
    integrator_state_write_mnee(
        state, shadow_state, &ls, &ray, mnee_vertex_count, mnee_throughput, mnee_wo);
  }

  return SHADER_EVAL_OK;
}

#endif /* __MNEE__ */

ccl_device void integrator_intersect_mnee(KernelGlobals kg, IntegratorState state)
{
  PROFILING_INIT(kg, PROFILING_SHADE_SURFACE_DIRECT_LIGHT);

  ShaderData sd;
  integrate_surface_shader_setup(kg, state, &sd);
  const int shader = sd.shader & SHADER_MASK;

#ifdef __MNEE__
  RNGState rng_state;
  path_state_rng_load(state, &rng_state);

  const ShaderEvalResult result = integrate_surface_mnee(kg, state, &sd, &rng_state);
  if (result == SHADER_EVAL_CACHE_MISS) {
    integrator_path_cache_miss(state, DEVICE_KERNEL_INTEGRATOR_INTERSECT_MNEE);
    return;
  }
#endif

  integrator_path_next_sorted(kg,
                              state,
                              DEVICE_KERNEL_INTEGRATOR_INTERSECT_MNEE,
                              DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE,
                              shader);
}

CCL_NAMESPACE_END
