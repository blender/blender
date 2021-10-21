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

#include "kernel/kernel_accumulate.h"
#include "kernel/kernel_emission.h"
#include "kernel/kernel_light.h"
#include "kernel/kernel_shader.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline void integrate_light(KernelGlobals kg,
                                       IntegratorState state,
                                       ccl_global float *ccl_restrict render_buffer)
{
  /* Setup light sample. */
  Intersection isect ccl_optional_struct_init;
  integrator_state_read_isect(kg, state, &isect);

  float3 ray_P = INTEGRATOR_STATE(state, ray, P);
  const float3 ray_D = INTEGRATOR_STATE(state, ray, D);
  const float ray_time = INTEGRATOR_STATE(state, ray, time);

  /* Advance ray beyond light. */
  /* TODO: can we make this more numerically robust to avoid reintersecting the
   * same light in some cases? */
  const float3 new_ray_P = ray_offset(ray_P + ray_D * isect.t, ray_D);
  INTEGRATOR_STATE_WRITE(state, ray, P) = new_ray_P;
  INTEGRATOR_STATE_WRITE(state, ray, t) -= isect.t;

  /* Set position to where the BSDF was sampled, for correct MIS PDF. */
  const float mis_ray_t = INTEGRATOR_STATE(state, path, mis_ray_t);
  ray_P -= ray_D * mis_ray_t;
  isect.t += mis_ray_t;
  INTEGRATOR_STATE_WRITE(state, path, mis_ray_t) = mis_ray_t + isect.t;

  LightSample ls ccl_optional_struct_init;
  const bool use_light_sample = light_sample_from_intersection(kg, &isect, ray_P, ray_D, &ls);

  if (!use_light_sample) {
    return;
  }

  /* Use visibility flag to skip lights. */
#ifdef __PASSES__
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  if (ls.shader & SHADER_EXCLUDE_ANY) {
    if (((ls.shader & SHADER_EXCLUDE_DIFFUSE) && (path_flag & PATH_RAY_DIFFUSE)) ||
        ((ls.shader & SHADER_EXCLUDE_GLOSSY) &&
         ((path_flag & (PATH_RAY_GLOSSY | PATH_RAY_REFLECT)) ==
          (PATH_RAY_GLOSSY | PATH_RAY_REFLECT))) ||
        ((ls.shader & SHADER_EXCLUDE_TRANSMIT) && (path_flag & PATH_RAY_TRANSMIT)) ||
        ((ls.shader & SHADER_EXCLUDE_SCATTER) && (path_flag & PATH_RAY_VOLUME_SCATTER)))
      return;
  }
#endif

  /* Evaluate light shader. */
  /* TODO: does aliasing like this break automatic SoA in CUDA? */
  ShaderDataTinyStorage emission_sd_storage;
  ccl_private ShaderData *emission_sd = AS_SHADER_DATA(&emission_sd_storage);
  float3 light_eval = light_sample_shader_eval(kg, state, emission_sd, &ls, ray_time);
  if (is_zero(light_eval)) {
    return;
  }

  /* MIS weighting. */
  if (!(path_flag & PATH_RAY_MIS_SKIP)) {
    /* multiple importance sampling, get regular light pdf,
     * and compute weight with respect to BSDF pdf */
    const float mis_ray_pdf = INTEGRATOR_STATE(state, path, mis_ray_pdf);
    const float mis_weight = power_heuristic(mis_ray_pdf, ls.pdf);
    light_eval *= mis_weight;
  }

  /* Write to render buffer. */
  const float3 throughput = INTEGRATOR_STATE(state, path, throughput);
  kernel_accum_emission(kg, state, throughput, light_eval, render_buffer);
}

ccl_device void integrator_shade_light(KernelGlobals kg,
                                       IntegratorState state,
                                       ccl_global float *ccl_restrict render_buffer)
{
  PROFILING_INIT(kg, PROFILING_SHADE_LIGHT_SETUP);

  integrate_light(kg, state, render_buffer);

  /* TODO: we could get stuck in an infinite loop if there are precision issues
   * and the same light is hit again.
   *
   * As a workaround count this as a transparent bounce. It makes some sense
   * to interpret lights as transparent surfaces (and support making them opaque),
   * but this needs to be revisited. */
  uint32_t transparent_bounce = INTEGRATOR_STATE(state, path, transparent_bounce) + 1;
  INTEGRATOR_STATE_WRITE(state, path, transparent_bounce) = transparent_bounce;

  if (transparent_bounce >= kernel_data.integrator.transparent_max_bounce) {
    INTEGRATOR_PATH_TERMINATE(DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT);
    return;
  }
  else {
    INTEGRATOR_PATH_NEXT(DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT,
                         DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);
    return;
  }

  /* TODO: in some cases we could continue directly to SHADE_BACKGROUND, but
   * probably that optimization is probably not practical if we add lights to
   * scene geometry. */
}

CCL_NAMESPACE_END
