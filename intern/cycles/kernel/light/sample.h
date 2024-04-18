/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/integrator/path_state.h"
#include "kernel/integrator/surface_shader.h"

#include "kernel/light/distribution.h"
#include "kernel/light/light.h"

#ifdef __LIGHT_TREE__
#  include "kernel/light/tree.h"
#endif

#include "kernel/sample/mapping.h"
#include "kernel/sample/mis.h"

CCL_NAMESPACE_BEGIN

/* Evaluate shader on light. */
ccl_device_noinline_cpu Spectrum
light_sample_shader_eval(KernelGlobals kg,
                         IntegratorState state,
                         ccl_private ShaderData *ccl_restrict emission_sd,
                         ccl_private LightSample *ccl_restrict ls,
                         float time)
{
  /* setup shading at emitter */
  Spectrum eval = zero_spectrum();

  if (surface_shader_constant_emission(kg, ls->shader, &eval)) {
    if ((ls->prim != PRIM_NONE) && dot(ls->Ng, ls->D) > 0.0f) {
      ls->Ng = -ls->Ng;
    }
  }
  else {
    /* Setup shader data and call surface_shader_eval once, better
     * for GPU coherence and compile times. */
    PROFILING_INIT_FOR_SHADER(kg, PROFILING_SHADE_LIGHT_SETUP);
    if (ls->type == LIGHT_BACKGROUND) {
      shader_setup_from_background(kg, emission_sd, ls->P, ls->D, time);
    }
    else {
      shader_setup_from_sample(kg,
                               emission_sd,
                               ls->P,
                               ls->Ng,
                               -ls->D,
                               ls->shader,
                               ls->object,
                               ls->prim,
                               ls->u,
                               ls->v,
                               ls->t,
                               time,
                               false,
                               ls->lamp);

      ls->Ng = emission_sd->Ng;
    }

    PROFILING_SHADER(emission_sd->object, emission_sd->shader);
    PROFILING_EVENT(PROFILING_SHADE_LIGHT_EVAL);

    /* No proper path flag, we're evaluating this for all closures. that's
     * weak but we'd have to do multiple evaluations otherwise. */
    surface_shader_eval<KERNEL_FEATURE_NODE_MASK_SURFACE_LIGHT>(
        kg, state, emission_sd, NULL, PATH_RAY_EMISSION);

    /* Evaluate closures. */
    if (ls->type == LIGHT_BACKGROUND) {
      eval = surface_shader_background(emission_sd);
    }
    else {
      eval = surface_shader_emission(emission_sd);
    }
  }

  eval *= ls->eval_fac;

  if (ls->lamp != LAMP_NONE) {
    ccl_global const KernelLight *klight = &kernel_data_fetch(lights, ls->lamp);
    eval *= rgb_to_spectrum(
        make_float3(klight->strength[0], klight->strength[1], klight->strength[2]));
  }

  return eval;
}

/* Early path termination of shadow rays. */
ccl_device_inline bool light_sample_terminate(KernelGlobals kg,
                                              ccl_private BsdfEval *ccl_restrict eval,
                                              const float rand_terminate)
{
  if (bsdf_eval_is_zero(eval)) {
    return true;
  }

  if (kernel_data.integrator.light_inv_rr_threshold > 0.0f) {
    float probability = reduce_max(fabs(bsdf_eval_sum(eval))) *
                        kernel_data.integrator.light_inv_rr_threshold;
    if (probability < 1.0f) {
      if (rand_terminate >= probability) {
        return true;
      }
      bsdf_eval_mul(eval, 1.0f / probability);
    }
  }

  return false;
}

/* This function should be used to compute a modified ray start position for
 * rays leaving from a surface. The algorithm slightly distorts flat surface
 * of a triangle. Surface is lifted by amount h along normal n in the incident
 * point. */

ccl_device_inline float3 shadow_ray_smooth_surface_offset(
    KernelGlobals kg, ccl_private const ShaderData *ccl_restrict sd, float3 Ng)
{
  float3 V[3], N[3];

  if (sd->type == PRIMITIVE_MOTION_TRIANGLE) {
    motion_triangle_vertices_and_normals(kg, sd->object, sd->prim, sd->time, V, N);
  }
  else {
    kernel_assert(sd->type == PRIMITIVE_TRIANGLE);
    triangle_vertices_and_normals(kg, sd->prim, V, N);
  }

  const float u = 1.0f - sd->u - sd->v;
  const float v = sd->u;
  const float w = sd->v;
  float3 P = V[0] * u + V[1] * v + V[2] * w; /* Local space */
  float3 n = N[0] * u + N[1] * v + N[2] * w; /* We get away without normalization */

  if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
    object_dir_transform(kg, sd, &n); /* Normal x scale, to world space */
  }

  /* Parabolic approximation */
  float a = dot(N[2] - N[0], V[0] - V[2]);
  float b = dot(N[2] - N[1], V[1] - V[2]);
  float c = dot(N[1] - N[0], V[1] - V[0]);
  float h = a * u * (u - 1) + (a + b + c) * u * v + b * v * (v - 1);

  /* Check flipped normals */
  if (dot(n, Ng) > 0) {
    /* Local linear envelope */
    float h0 = max(max(dot(V[1] - V[0], N[0]), dot(V[2] - V[0], N[0])), 0.0f);
    float h1 = max(max(dot(V[0] - V[1], N[1]), dot(V[2] - V[1], N[1])), 0.0f);
    float h2 = max(max(dot(V[0] - V[2], N[2]), dot(V[1] - V[2], N[2])), 0.0f);
    h0 = max(dot(V[0] - P, N[0]) + h0, 0.0f);
    h1 = max(dot(V[1] - P, N[1]) + h1, 0.0f);
    h2 = max(dot(V[2] - P, N[2]) + h2, 0.0f);
    h = max(min(min(h0, h1), h2), h * 0.5f);
  }
  else {
    float h0 = max(max(dot(V[0] - V[1], N[0]), dot(V[0] - V[2], N[0])), 0.0f);
    float h1 = max(max(dot(V[1] - V[0], N[1]), dot(V[1] - V[2], N[1])), 0.0f);
    float h2 = max(max(dot(V[2] - V[0], N[2]), dot(V[2] - V[1], N[2])), 0.0f);
    h0 = max(dot(P - V[0], N[0]) + h0, 0.0f);
    h1 = max(dot(P - V[1], N[1]) + h1, 0.0f);
    h2 = max(dot(P - V[2], N[2]) + h2, 0.0f);
    h = min(-min(min(h0, h1), h2), h * 0.5f);
  }

  return n * h;
}

/* Ray offset to avoid shadow terminator artifact. */

ccl_device_inline float3 shadow_ray_offset(KernelGlobals kg,
                                           ccl_private const ShaderData *ccl_restrict sd,
                                           float3 L,
                                           ccl_private bool *r_skip_self)
{
  float3 P = sd->P;

  if ((sd->type & PRIMITIVE_TRIANGLE) && (sd->shader & SHADER_SMOOTH_NORMAL)) {
    const float offset_cutoff =
        kernel_data_fetch(objects, sd->object).shadow_terminator_geometry_offset;
    /* Do ray offset (heavy stuff) only for close to be terminated triangles:
     * offset_cutoff = 0.1f means that 10-20% of rays will be affected. Also
     * make a smooth transition near the threshold. */
    if (offset_cutoff > 0.0f) {
      float NL = dot(sd->N, L);
      const bool transmit = (NL < 0.0f);
      if (NL < 0) {
        NL = -NL;
      }

      const float3 Ng = (transmit ? -sd->Ng : sd->Ng);
      const float NgL = dot(Ng, L);

      const float offset_amount = (NL < offset_cutoff) ?
                                      clamp(2.0f - (NgL + NL) / offset_cutoff, 0.0f, 1.0f) :
                                      clamp(1.0f - NgL / offset_cutoff, 0.0f, 1.0f);

      if (offset_amount > 0.0f) {
        P += shadow_ray_smooth_surface_offset(kg, sd, Ng) * offset_amount;

        /* Only skip self intersections if light direction and geometric normal point in the same
         * direction, otherwise we're meant to hit this surface. */
        *r_skip_self = (NgL > 0.0f);
      }
    }
  }

  return P;
}

ccl_device_inline void shadow_ray_setup(ccl_private const ShaderData *ccl_restrict sd,
                                        ccl_private const LightSample *ccl_restrict ls,
                                        const float3 P,
                                        ccl_private Ray *ray,
                                        const bool skip_self)
{
  if (ls->shader & SHADER_CAST_SHADOW) {
    /* setup ray */
    ray->P = P;
    ray->tmin = 0.0f;

    if (ls->t == FLT_MAX) {
      /* distant light */
      ray->D = ls->D;
      ray->tmax = ls->t;
    }
    else {
      /* other lights, avoid self-intersection */
      ray->D = ls->P - P;
      ray->D = normalize_len(ray->D, &ray->tmax);
    }
  }
  else {
    /* signal to not cast shadow ray */
    ray->P = zero_float3();
    ray->D = zero_float3();
    ray->tmax = 0.0f;
  }

  ray->dP = differential_make_compact(sd->dP);
  ray->dD = differential_zero_compact();
  ray->time = sd->time;

  /* Fill in intersection surface and light details. */
  ray->self.object = (skip_self) ? sd->object : OBJECT_NONE;
  ray->self.prim = (skip_self) ? sd->prim : PRIM_NONE;
  ray->self.light_object = ls->object;
  ray->self.light_prim = ls->prim;
  ray->self.light = ls->lamp;
}

/* Create shadow ray towards light sample. */
ccl_device_inline void light_sample_to_surface_shadow_ray(
    KernelGlobals kg,
    ccl_private const ShaderData *ccl_restrict sd,
    ccl_private const LightSample *ccl_restrict ls,
    ccl_private Ray *ray)
{
  bool skip_self = true;
  const float3 P = shadow_ray_offset(kg, sd, ls->D, &skip_self);
  shadow_ray_setup(sd, ls, P, ray, skip_self);
}

/* Create shadow ray towards light sample. */
ccl_device_inline void light_sample_to_volume_shadow_ray(
    KernelGlobals kg,
    ccl_private const ShaderData *ccl_restrict sd,
    ccl_private const LightSample *ccl_restrict ls,
    const float3 P,
    ccl_private Ray *ray)
{
  shadow_ray_setup(sd, ls, P, ray, false);
}

/* Multiple importance sampling weights. */

ccl_device_inline float light_sample_mis_weight_forward(KernelGlobals kg,
                                                        const float forward_pdf,
                                                        const float nee_pdf)
{
#ifdef WITH_CYCLES_DEBUG
  if (kernel_data.integrator.direct_light_sampling_type == DIRECT_LIGHT_SAMPLING_FORWARD) {
    return 1.0f;
  }
  else if (kernel_data.integrator.direct_light_sampling_type == DIRECT_LIGHT_SAMPLING_NEE) {
    return 0.0f;
  }
  else
#endif
    return power_heuristic(forward_pdf, nee_pdf);
}

ccl_device_inline float light_sample_mis_weight_nee(KernelGlobals kg,
                                                    const float nee_pdf,
                                                    const float forward_pdf)
{
#ifdef WITH_CYCLES_DEBUG
  if (kernel_data.integrator.direct_light_sampling_type == DIRECT_LIGHT_SAMPLING_FORWARD) {
    /* Return 0.0f to only account for the contribution in forward path tracing, unless when the
     * light can not be forward sampled, in which case return 1.0f so it converges to the same
     * result. */
    return (forward_pdf == 0.0f);
  }
  else if (kernel_data.integrator.direct_light_sampling_type == DIRECT_LIGHT_SAMPLING_NEE) {
    return 1.0f;
  }
  else
#endif
    return power_heuristic(nee_pdf, forward_pdf);
}

/* Next event estimation sampling.
 *
 * Sample a position on a light in the scene, from a position on a surface or
 * from a volume segment.
 *
 * Uses either a flat distribution or light tree. */

ccl_device_inline bool light_sample_from_volume_segment(KernelGlobals kg,
                                                        const float3 rand,
                                                        const float time,
                                                        const float3 P,
                                                        const float3 D,
                                                        const float t,
                                                        const int object_receiver,
                                                        const int bounce,
                                                        const uint32_t path_flag,
                                                        ccl_private LightSample *ls)
{
  const int shader_flags = SD_BSDF_HAS_TRANSMISSION;

#ifdef __LIGHT_TREE__
  if (kernel_data.integrator.use_light_tree) {
    if (!light_tree_sample<true>(kg, rand.z, P, D, t, object_receiver, shader_flags, ls)) {
      return false;
    }
  }
  else
#endif
  {
    if (!light_distribution_sample(kg, rand.z, ls)) {
      return false;
    }
  }

  /* Sample position on the selected light. */
  return light_sample<true>(
      kg, rand, time, P, D, object_receiver, shader_flags, bounce, path_flag, ls);
}

ccl_device bool light_sample_from_position(KernelGlobals kg,
                                           const float3 rand,
                                           const float time,
                                           const float3 P,
                                           const float3 N,
                                           const int object_receiver,
                                           const int shader_flags,
                                           const int bounce,
                                           const uint32_t path_flag,
                                           ccl_private LightSample *ls)
{
  /* Randomly select a light. */
#ifdef __LIGHT_TREE__
  if (kernel_data.integrator.use_light_tree) {
    if (!light_tree_sample<false>(kg, rand.z, P, N, 0.0f, object_receiver, shader_flags, ls)) {
      return false;
    }
  }
  else
#endif
  {
    if (!light_distribution_sample(kg, rand.z, ls)) {
      return false;
    }
  }

  /* Sample position on the selected light. */
  return light_sample<false>(
      kg, rand, time, P, N, object_receiver, shader_flags, bounce, path_flag, ls);
}

/* Update light sample with new shading point position for MNEE. The position on the light is fixed
 * except for directional light. */
ccl_device_forceinline void light_sample_update(KernelGlobals kg,
                                                ccl_private LightSample *ls,
                                                const float3 P,
                                                const float3 N,
                                                const uint32_t path_flag)
{
  const ccl_global KernelLight *klight = &kernel_data_fetch(lights, ls->lamp);

  if (ls->type == LIGHT_POINT) {
    point_light_mnee_sample_update(klight, ls, P, N, path_flag);
  }
  else if (ls->type == LIGHT_SPOT) {
    spot_light_mnee_sample_update(klight, ls, P, N, path_flag);
  }
  else if (ls->type == LIGHT_AREA) {
    area_light_mnee_sample_update(klight, ls, P);
  }
  else {
    /* Keep previous values. */
  }

  /* Re-apply already computed selection pdf. */
  ls->pdf *= ls->pdf_selection;
}

/* Forward sampling.
 *
 * Multiple importance sampling weights for hitting surface, light or background
 * through indirect light ray.
 *
 * The BSDF or phase pdf from the previous bounce was stored in mis_ray_pdf and
 * is used for balancing with the light sampling pdf. */

ccl_device_inline float light_sample_mis_weight_forward_surface(KernelGlobals kg,
                                                                IntegratorState state,
                                                                const uint32_t path_flag,
                                                                const ccl_private ShaderData *sd)
{
  bool has_mis = !(path_flag & PATH_RAY_MIS_SKIP) &&
                 (sd->flag & ((sd->flag & SD_BACKFACING) ? SD_MIS_BACK : SD_MIS_FRONT));

#ifdef __HAIR__
  has_mis &= (sd->type & PRIMITIVE_TRIANGLE);
#endif

  if (!has_mis) {
    return 1.0f;
  }

  const float bsdf_pdf = INTEGRATOR_STATE(state, path, mis_ray_pdf);
  const float t = sd->ray_length;
  float pdf = triangle_light_pdf(kg, sd, t);

  /* Light selection pdf. */
#ifdef __LIGHT_TREE__
  if (kernel_data.integrator.use_light_tree) {
    float3 ray_P = INTEGRATOR_STATE(state, ray, P);
    const float dt = INTEGRATOR_STATE(state, ray, previous_dt);
    const float3 N = INTEGRATOR_STATE(state, path, mis_origin_n);

    uint lookup_offset = kernel_data_fetch(object_lookup_offset, sd->object);
    uint prim_offset = kernel_data_fetch(object_prim_offset, sd->object);
    uint triangle = kernel_data_fetch(triangle_to_tree, sd->prim - prim_offset + lookup_offset);

    pdf *= light_tree_pdf(
        kg, ray_P, N, dt, path_flag, sd->object, triangle, light_link_receiver_forward(kg, state));
  }
  else
#endif
  {
    /* Handled in triangle_light_pdf for efficiency. */
  }

  return light_sample_mis_weight_forward(kg, bsdf_pdf, pdf);
}

ccl_device_inline float light_sample_mis_weight_forward_lamp(KernelGlobals kg,
                                                             IntegratorState state,
                                                             const uint32_t path_flag,
                                                             const ccl_private LightSample *ls,
                                                             const float3 P)
{
  if (path_flag & PATH_RAY_MIS_SKIP) {
    return 1.0f;
  }

  const float mis_ray_pdf = INTEGRATOR_STATE(state, path, mis_ray_pdf);
  float pdf = ls->pdf;

  /* Light selection pdf. */
#ifdef __LIGHT_TREE__
  if (kernel_data.integrator.use_light_tree) {
    const float3 N = INTEGRATOR_STATE(state, path, mis_origin_n);
    const float dt = INTEGRATOR_STATE(state, ray, previous_dt);
    pdf *= light_tree_pdf(kg,
                          P,
                          N,
                          dt,
                          path_flag,
                          0,
                          kernel_data_fetch(light_to_tree, ls->lamp),
                          light_link_receiver_forward(kg, state));
  }
  else
#endif
  {
    pdf *= light_distribution_pdf_lamp(kg);
  }

  return light_sample_mis_weight_forward(kg, mis_ray_pdf, pdf);
}

ccl_device_inline float light_sample_mis_weight_forward_distant(KernelGlobals kg,
                                                                IntegratorState state,
                                                                const uint32_t path_flag,
                                                                const ccl_private LightSample *ls)
{
  const float3 ray_P = INTEGRATOR_STATE(state, ray, P);
  return light_sample_mis_weight_forward_lamp(kg, state, path_flag, ls, ray_P);
}

ccl_device_inline float light_sample_mis_weight_forward_background(KernelGlobals kg,
                                                                   IntegratorState state,
                                                                   const uint32_t path_flag)
{
  /* Check if background light exists or if we should skip PDF. */
  if (!kernel_data.background.use_mis || (path_flag & PATH_RAY_MIS_SKIP)) {
    return 1.0f;
  }

  const float3 ray_P = INTEGRATOR_STATE(state, ray, P);
  const float3 ray_D = INTEGRATOR_STATE(state, ray, D);
  const float mis_ray_pdf = INTEGRATOR_STATE(state, path, mis_ray_pdf);

  float pdf = background_light_pdf(kg, ray_P, ray_D);

  /* Light selection pdf. */
#ifdef __LIGHT_TREE__
  if (kernel_data.integrator.use_light_tree) {
    const float3 N = INTEGRATOR_STATE(state, path, mis_origin_n);
    const float dt = INTEGRATOR_STATE(state, ray, previous_dt);
    uint light = kernel_data_fetch(light_to_tree, kernel_data.background.light_index);
    pdf *= light_tree_pdf(
        kg, ray_P, N, dt, path_flag, 0, light, light_link_receiver_forward(kg, state));
  }
  else
#endif
  {
    pdf *= light_distribution_pdf_lamp(kg);
  }

  return light_sample_mis_weight_forward(kg, mis_ray_pdf, pdf);
}

CCL_NAMESPACE_END
