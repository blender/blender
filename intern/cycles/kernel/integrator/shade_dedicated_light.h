/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/light/distant.h"
#include "kernel/light/light.h"
#include "kernel/light/sample.h"

#include "kernel/integrator/shade_surface.h"

CCL_NAMESPACE_BEGIN

#ifdef __SHADOW_LINKING__

ccl_device_inline LightEval
shadow_linking_light_eval_from_intersection(KernelGlobals kg,
                                            const ccl_private Intersection &ccl_restrict isect,
                                            const ccl_private Ray &ccl_restrict ray,
                                            const float3 N,
                                            const uint32_t path_flag)
{
  const ccl_global KernelLight *klight = &kernel_data_fetch(lights, isect.prim);
  const LightType type = LightType(klight->type);

  return (type == LIGHT_DISTANT) ?
             distant_light_eval_from_intersection(klight, ray.D) :
             light_eval_from_intersection(kg, &isect, ray.P, ray.D, N, path_flag);
}

ccl_device_inline float shadow_linking_light_sample_mis_weight(KernelGlobals kg,
                                                               IntegratorState state,
                                                               const uint32_t path_flag,
                                                               const int light_id,
                                                               const float light_sample_pdf,
                                                               const float3 P)
{
  if (kernel_data_fetch(lights, light_id).type == LIGHT_DISTANT) {
    return light_sample_mis_weight_forward_distant(
        kg, state, path_flag, light_id, light_sample_pdf);
  }

  return light_sample_mis_weight_forward_lamp(kg, state, path_flag, light_id, light_sample_pdf, P);
}

/* Setup ray for the shadow path.
 * Expects that the current state of the ray is the one calculated by the surface bounce, and the
 * intersection corresponds to a point on an emitter. */
ccl_device void shadow_linking_setup_ray_from_intersection(
    IntegratorState state,
    ccl_private Ray *ccl_restrict ray,
    const ccl_private Intersection *ccl_restrict isect)
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

  ray->self.light_object = isect->object;
  ray->self.light_prim = isect->prim;
}

ccl_device bool shadow_linking_shade_light(KernelGlobals kg,
                                           IntegratorState state,
                                           ccl_private Ray &ccl_restrict ray,
                                           ccl_private Intersection &ccl_restrict isect,
                                           ccl_private float &ccl_restrict light_weight,
                                           ccl_private float &mis_weight,
                                           ccl_private int &ccl_restrict light_group,
                                           ccl_private int &ccl_restrict shader_id)
{
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  const float3 N = INTEGRATOR_STATE(state, path, mis_origin_n);
  const LightEval light_eval = shadow_linking_light_eval_from_intersection(
      kg, isect, ray, N, path_flag);
  if (light_eval.eval_fac == 0.0f) {
    /* No light to be sampled, so no direct light contribution either. */
    return false;
  }

  const ccl_global KernelLight *klight = &kernel_data_fetch(lights, isect.prim);

  if (!is_light_shader_visible_to_path(klight->shader_id, path_flag)) {
    return false;
  }

  /* MIS weighting. */
  mis_weight = shadow_linking_light_sample_mis_weight(
      kg, state, path_flag, isect.prim, light_eval.pdf, ray.P);

  light_weight = light_eval.eval_fac * mis_weight *
                 INTEGRATOR_STATE(state, shadow_link, dedicated_light_weight);
  light_group = object_lightgroup(kg, klight->object_id);
  shader_id = klight->shader_id;

  return true;
}

ccl_device bool shadow_linking_shade_surface_emission(KernelGlobals kg,
                                                      IntegratorState state,
                                                      ccl_private float &ccl_restrict light_weight,
                                                      ccl_private float &mis_weight,
                                                      ccl_private int &ccl_restrict light_group,
                                                      ccl_private int &ccl_restrict shader_id)
{
  ShaderDataTinyStorage emission_sd_storage;
  ccl_private ShaderData *emission_sd = AS_SHADER_DATA(&emission_sd_storage);

  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  integrate_surface_shader_setup(kg, state, emission_sd);

#  ifdef __VOLUME__
  if (emission_sd->flag & SD_HAS_ONLY_VOLUME) {
    return false;
  }
#  endif

  mis_weight = light_sample_mis_weight_forward_surface(kg, state, path_flag, emission_sd);

  light_weight = mis_weight * INTEGRATOR_STATE(state, shadow_link, dedicated_light_weight);
  light_group = object_lightgroup(kg, emission_sd->object);
  shader_id = emission_sd->shader;

  return true;
}

ccl_device void shadow_linking_shade(KernelGlobals kg, IntegratorState state)
{
  /* Read intersection from integrator state into local memory. */
  Intersection isect ccl_optional_struct_init;
  integrator_state_read_isect(state, &isect);

  /* Read ray from integrator state into local memory. */
  Ray ray ccl_optional_struct_init;
  integrator_state_read_ray(state, &ray);

  float light_weight = 0.0f;
  float mis_weight = 1.0f;
  int light_group = LIGHTGROUP_NONE;
  int shader_id = SHADER_NONE;

  if (isect.type == PRIMITIVE_LAMP) {
    if (!shadow_linking_shade_light(
            kg, state, ray, isect, light_weight, mis_weight, light_group, shader_id))
    {
      return;
    }
  }
  else {
    if (!shadow_linking_shade_surface_emission(
            kg, state, light_weight, mis_weight, light_group, shader_id))
    {
      return;
    }
  }

  /* Evaluate constant part of light shader, rest will optionally be done in another kernel. */
  Spectrum light_eval;
  const bool is_constant_light_shader = light_sample_shader_eval_nee_constant(
      kg, shader_id, isect.prim, isect.type == PRIMITIVE_LAMP, light_eval);
  light_eval *= light_weight;

  if (is_zero(light_eval)) {
    return;
  }

  shadow_linking_setup_ray_from_intersection(state, &ray, &isect);

  /* Branch off shadow kernel. */
  IntegratorShadowState shadow_state = integrate_direct_light_shadow_init_common(
      kg, state, &ray, light_eval, light_group, 0, is_constant_light_shader);

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

#  if defined(__PATH_GUIDING__)
  if (kernel_data.integrator.train_guiding) {
    guiding_record_light_surface_segment(kg, state, &isect);
    INTEGRATOR_STATE(shadow_state, shadow_path, guiding_mis_weight) = mis_weight;
  }
#  endif
}

#endif /* __SHADOW_LINKING__ */

ccl_device void integrator_shade_dedicated_light(KernelGlobals kg,
                                                 IntegratorState state,
                                                 ccl_global float *ccl_restrict /*render_buffer*/)
{
  PROFILING_INIT(kg, PROFILING_SHADE_DEDICATED_LIGHT);

#ifdef __SHADOW_LINKING__
  shadow_linking_shade(kg, state);

  /* Restore self-intersection check primitives in the main state before returning to the
   * intersect_closest() state. */
  shadow_linking_restore_last_primitives(state);
#else
  kernel_assert(!"integrator_intersect_dedicated_light is not supposed to be scheduled");
#endif

  integrator_shade_surface_next_kernel<DEVICE_KERNEL_INTEGRATOR_SHADE_DEDICATED_LIGHT>(state);
}

CCL_NAMESPACE_END
