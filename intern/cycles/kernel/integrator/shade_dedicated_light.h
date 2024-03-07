/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/integrator/path_state.h"

#include "kernel/light/distant.h"
#include "kernel/light/light.h"
#include "kernel/light/sample.h"

#include "kernel/integrator/shade_surface.h"

CCL_NAMESPACE_BEGIN

#ifdef __SHADOW_LINKING__

ccl_device_inline bool shadow_linking_light_sample_from_intersection(
    KernelGlobals kg,
    ccl_private const Intersection &ccl_restrict isect,
    ccl_private const Ray &ccl_restrict ray,
    const float3 N,
    const uint32_t path_flag,
    ccl_private LightSample *ccl_restrict ls)
{
  const int lamp = isect.prim;

  const ccl_global KernelLight *klight = &kernel_data_fetch(lights, lamp);
  const LightType type = LightType(klight->type);

  if (type == LIGHT_DISTANT) {
    return distant_light_sample_from_intersection(kg, ray.D, lamp, ls);
  }

  return light_sample_from_intersection(kg, &isect, ray.P, ray.D, N, path_flag, ls);
}

ccl_device_inline float shadow_linking_light_sample_mis_weight(KernelGlobals kg,
                                                               IntegratorState state,
                                                               const uint32_t path_flag,
                                                               const ccl_private LightSample *ls,
                                                               const float3 P)
{
  if (ls->type == LIGHT_DISTANT) {
    return light_sample_mis_weight_forward_distant(kg, state, path_flag, ls);
  }

  return light_sample_mis_weight_forward_lamp(kg, state, path_flag, ls, P);
}

/* Setup ray for the shadow path.
 * Expects that the current state of the ray is the one calculated by the surface bounce, and the
 * intersection corresponds to a point on an emitter. */
ccl_device void shadow_linking_setup_ray_from_intersection(
    IntegratorState state,
    ccl_private Ray *ccl_restrict ray,
    ccl_private const Intersection *ccl_restrict isect)
{
  /* The ray->tmin follows the value configured at the surface bounce.
   * it is the same for the continued main path and for this shadow ray. There is no need to push
   * it forward here. */

  ray->tmax = isect->t;

  /* Use the same self intersection primitives as the main path.
   * Those are copied to the dedicated storage from the main intersection after the surface bounce,
   * but before the main intersection is re-used to find light to trace a ray to. */
  ray->self.object = INTEGRATOR_STATE(state, shadow_link, last_isect_object);
  ray->self.prim = INTEGRATOR_STATE(state, shadow_link, last_isect_prim);

  if (isect->type == PRIMITIVE_LAMP) {
    ray->self.light_object = OBJECT_NONE;
    ray->self.light_prim = PRIM_NONE;
    ray->self.light = isect->prim;
  }
  else {
    ray->self.light_object = isect->object;
    ray->self.light_prim = isect->prim;
    ray->self.light = LAMP_NONE;
  }
}

ccl_device bool shadow_linking_shade_light(KernelGlobals kg,
                                           IntegratorState state,
                                           ccl_private Ray &ccl_restrict ray,
                                           ccl_private Intersection &ccl_restrict isect,
                                           ccl_private ShaderData *emission_sd,
                                           ccl_private Spectrum &ccl_restrict bsdf_spectrum,
                                           ccl_private float &mis_weight,
                                           ccl_private int &ccl_restrict light_group)
{
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  const float3 N = INTEGRATOR_STATE(state, path, mis_origin_n);
  LightSample ls ccl_optional_struct_init;
  const bool use_light_sample = shadow_linking_light_sample_from_intersection(
      kg, isect, ray, N, path_flag, &ls);
  if (!use_light_sample) {
    /* No light to be sampled, so no direct light contribution either. */
    return false;
  }

  const Spectrum light_eval = light_sample_shader_eval(kg, state, emission_sd, &ls, ray.time);
  if (is_zero(light_eval)) {
    return false;
  }

  if (!is_light_shader_visible_to_path(ls.shader, path_flag)) {
    return false;
  }

  /* MIS weighting. */
  mis_weight = shadow_linking_light_sample_mis_weight(kg, state, path_flag, &ls, ray.P);

  bsdf_spectrum = light_eval * mis_weight *
                  INTEGRATOR_STATE(state, shadow_link, dedicated_light_weight);

  // TODO(: De-duplicate with the shade_surface.
  // Possibly by ensuring ls->group is always assigned properly.
  light_group = ls.type != LIGHT_BACKGROUND ? ls.group : kernel_data.background.lightgroup;

  return true;
}

ccl_device bool shadow_linking_shade_surface_emission(KernelGlobals kg,
                                                      IntegratorState state,
                                                      ccl_private ShaderData *emission_sd,
                                                      ccl_global float *ccl_restrict render_buffer,
                                                      ccl_private Spectrum &ccl_restrict
                                                          bsdf_spectrum,
                                                      ccl_private float &mis_weight,
                                                      ccl_private int &ccl_restrict light_group)
{
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  integrate_surface_shader_setup(kg, state, emission_sd);

#  ifdef __VOLUME__
  if (emission_sd->flag & SD_HAS_ONLY_VOLUME) {
    return false;
  }
#  endif

  surface_shader_eval<KERNEL_FEATURE_NODE_MASK_SURFACE_LIGHT>(
      kg, state, emission_sd, render_buffer, path_flag | PATH_RAY_EMISSION);

  if ((emission_sd->flag & SD_EMISSION) == 0) {
    return false;
  }

  const Spectrum L = surface_shader_emission(emission_sd);

  mis_weight = light_sample_mis_weight_forward_surface(kg, state, path_flag, emission_sd);

  bsdf_spectrum = L * mis_weight * INTEGRATOR_STATE(state, shadow_link, dedicated_light_weight);
  light_group = object_lightgroup(kg, emission_sd->object);

  return true;
}

ccl_device void shadow_linking_shade(KernelGlobals kg,
                                     IntegratorState state,
                                     ccl_global float *ccl_restrict render_buffer)
{
  /* Read intersection from integrator state into local memory. */
  Intersection isect ccl_optional_struct_init;
  integrator_state_read_isect(state, &isect);

  /* Read ray from integrator state into local memory. */
  Ray ray ccl_optional_struct_init;
  integrator_state_read_ray(state, &ray);

  ShaderDataTinyStorage emission_sd_storage;
  ccl_private ShaderData *emission_sd = AS_SHADER_DATA(&emission_sd_storage);

  Spectrum bsdf_spectrum;
  float mis_weight = 1.0f;
  int light_group = LIGHTGROUP_NONE;

  if (isect.type == PRIMITIVE_LAMP) {
    if (!shadow_linking_shade_light(
            kg, state, ray, isect, emission_sd, bsdf_spectrum, mis_weight, light_group))
    {
      return;
    }
  }
  else {
    if (!shadow_linking_shade_surface_emission(
            kg, state, emission_sd, render_buffer, bsdf_spectrum, mis_weight, light_group))
    {
      return;
    }
  }

  if (is_zero(bsdf_spectrum)) {
    return;
  }

  shadow_linking_setup_ray_from_intersection(state, &ray, &isect);

  /* Branch off shadow kernel. */
  IntegratorShadowState shadow_state = integrate_direct_light_shadow_init_common(
      kg, state, &ray, bsdf_spectrum, light_group, 0);

  /* The light is accumulated from the shade_surface kernel, which will make the clamping decision
   * based on the actual value of the bounce. For the dedicated shadow ray we want to follow the
   * main path clamping rules, which subtracts one from the bounds before accumulation. */
  INTEGRATOR_STATE_WRITE(
      shadow_state, shadow_path, bounce) = INTEGRATOR_STATE(shadow_state, shadow_path, bounce) - 1;

  /* No need to update the volume stack as the surface bounce already performed enter-exit check.
   */

  const uint32_t shadow_flag = INTEGRATOR_STATE(state, path, flag);

  if (kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_PASSES) {
    /* The diffuse and glossy pass weights are written into the main path as part of the path
     * configuration at a surface bounce. */
    INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, pass_diffuse_weight) = INTEGRATOR_STATE(
        state, path, pass_diffuse_weight);
    INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, pass_glossy_weight) = INTEGRATOR_STATE(
        state, path, pass_glossy_weight);
  }

  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, flag) = shadow_flag;

#  ifdef __PATH_GUIDING__
  if (kernel_data.integrator.train_guiding) {
    guiding_record_light_surface_segment(kg, state, &isect);
    INTEGRATOR_STATE(shadow_state, shadow_path, guiding_mis_weight) = mis_weight;
  }
#  endif
}

#endif /* __SHADOW_LINKING__ */

ccl_device void integrator_shade_dedicated_light(KernelGlobals kg,
                                                 IntegratorState state,
                                                 ccl_global float *ccl_restrict render_buffer)
{
  PROFILING_INIT(kg, PROFILING_SHADE_DEDICATED_LIGHT);

#ifdef __SHADOW_LINKING__
  shadow_linking_shade(kg, state, render_buffer);

  /* Restore self-intersection check primitives in the main state before returning to the
   * intersect_closest() state. */
  shadow_linking_restore_last_primitives(state);
#else
  kernel_assert(!"integrator_intersect_dedicated_light is not supposed to be scheduled");
#endif

  integrator_shade_surface_next_kernel<DEVICE_KERNEL_INTEGRATOR_SHADE_DEDICATED_LIGHT>(kg, state);
}

CCL_NAMESPACE_END
