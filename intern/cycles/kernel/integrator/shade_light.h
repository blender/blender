/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/film/light_passes.h"
#include "kernel/integrator/surface_shader.h"
#include "kernel/light/light.h"
#include "kernel/light/sample.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline void integrate_light(KernelGlobals kg,
                                       IntegratorState state,
                                       ccl_global float *ccl_restrict render_buffer)
{
  /* Setup light sample. */
  Intersection isect ccl_optional_struct_init;
  integrator_state_read_isect(state, &isect);

  guiding_record_light_surface_segment(kg, state, &isect);

  float3 ray_P = INTEGRATOR_STATE(state, ray, P);
  const float3 ray_D = INTEGRATOR_STATE(state, ray, D);
  const float ray_time = INTEGRATOR_STATE(state, ray, time);

  /* Advance ray to new start distance. */
  INTEGRATOR_STATE_WRITE(state, ray, tmin) = intersection_t_offset(isect.t);

  LightSample ls ccl_optional_struct_init;
  const bool use_light_sample = light_sample_from_intersection(kg, &isect, ray_P, ray_D, &ls);

  if (!use_light_sample) {
    return;
  }

  /* Use visibility flag to skip lights. */
#ifdef __PASSES__
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  if (!is_light_shader_visible_to_path(ls.shader, path_flag)) {
    return;
  }
#endif

  /* Evaluate light shader. */
  /* TODO: does aliasing like this break automatic SoA in CUDA? */
  ShaderDataTinyStorage emission_sd_storage;
  ccl_private ShaderData *emission_sd = AS_SHADER_DATA(&emission_sd_storage);
  Spectrum light_eval = light_sample_shader_eval(kg, state, emission_sd, &ls, ray_time);
  if (is_zero(light_eval)) {
    return;
  }

  /* MIS weighting. */
  float mis_weight = 1.0f;
  if (!(path_flag & PATH_RAY_MIS_SKIP)) {
    mis_weight = light_sample_mis_weight_forward_lamp(kg, state, path_flag, &ls, ray_P);
  }

  /* Write to render buffer. */
  guiding_record_surface_emission(kg, state, light_eval, mis_weight);
  film_write_surface_emission(kg, state, light_eval, mis_weight, render_buffer, ls.group);
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
    integrator_path_terminate(kg, state, DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT);
    return;
  }
  else {
    integrator_path_next(kg,
                         state,
                         DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT,
                         DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);
    return;
  }

  /* TODO: in some cases we could continue directly to SHADE_BACKGROUND, but
   * probably that optimization is probably not practical if we add lights to
   * scene geometry. */
}

CCL_NAMESPACE_END
