/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/integrator/path_state.h"
#include "kernel/integrator/shader_eval.h"

#include "kernel/light/light.h"

#include "kernel/sample/mapping.h"
#include "kernel/sample/mis.h"

CCL_NAMESPACE_BEGIN

/* Evaluate shader on light. */
ccl_device_noinline_cpu float3
light_sample_shader_eval(KernelGlobals kg,
                         IntegratorState state,
                         ccl_private ShaderData *ccl_restrict emission_sd,
                         ccl_private LightSample *ccl_restrict ls,
                         float time)
{
  /* setup shading at emitter */
  float3 eval = zero_float3();

  if (shader_constant_emission_eval(kg, ls->shader, &eval)) {
    if ((ls->prim != PRIM_NONE) && dot(ls->Ng, ls->D) > 0.0f) {
      ls->Ng = -ls->Ng;
    }
  }
  else {
    /* Setup shader data and call shader_eval_surface once, better
     * for GPU coherence and compile times. */
    PROFILING_INIT_FOR_SHADER(kg, PROFILING_SHADE_LIGHT_SETUP);
#ifdef __BACKGROUND_MIS__
    if (ls->type == LIGHT_BACKGROUND) {
      shader_setup_from_background(kg, emission_sd, ls->P, ls->D, time);
    }
    else
#endif
    {
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
    shader_eval_surface<KERNEL_FEATURE_NODE_MASK_SURFACE_LIGHT>(
        kg, state, emission_sd, NULL, PATH_RAY_EMISSION);

    /* Evaluate closures. */
#ifdef __BACKGROUND_MIS__
    if (ls->type == LIGHT_BACKGROUND) {
      eval = shader_background_eval(emission_sd);
    }
    else
#endif
    {
      eval = shader_emissive_eval(emission_sd);
    }
  }

  eval *= ls->eval_fac;

  if (ls->lamp != LAMP_NONE) {
    ccl_global const KernelLight *klight = &kernel_tex_fetch(__lights, ls->lamp);
    eval *= make_float3(klight->strength[0], klight->strength[1], klight->strength[2]);
  }

  return eval;
}

/* Test if light sample is from a light or emission from geometry. */
ccl_device_inline bool light_sample_is_light(ccl_private const LightSample *ccl_restrict ls)
{
  /* return if it's a lamp for shadow pass */
  return (ls->prim == PRIM_NONE && ls->type != LIGHT_BACKGROUND);
}

/* Early path termination of shadow rays. */
ccl_device_inline bool light_sample_terminate(KernelGlobals kg,
                                              ccl_private const LightSample *ccl_restrict ls,
                                              ccl_private BsdfEval *ccl_restrict eval,
                                              const float rand_terminate)
{
  if (bsdf_eval_is_zero(eval)) {
    return true;
  }

  if (kernel_data.integrator.light_inv_rr_threshold > 0.0f) {
    float probability = max3(fabs(bsdf_eval_sum(eval))) *
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

  const float u = sd->u, v = sd->v;
  const float w = 1 - u - v;
  float3 P = V[0] * u + V[1] * v + V[2] * w; /* Local space */
  float3 n = N[0] * u + N[1] * v + N[2] * w; /* We get away without normalization */

  if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
    object_normal_transform(kg, sd, &n); /* Normal x scale, world space */
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
        kernel_tex_fetch(__objects, sd->object).shadow_terminator_geometry_offset;
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

    if (ls->t == FLT_MAX) {
      /* distant light */
      ray->D = ls->D;
      ray->t = ls->t;
    }
    else {
      /* other lights, avoid self-intersection */
      ray->D = ls->P - P;
      ray->D = normalize_len(ray->D, &ray->t);
    }
  }
  else {
    /* signal to not cast shadow ray */
    ray->P = zero_float3();
    ray->D = zero_float3();
    ray->t = 0.0f;
  }

  ray->dP = differential_make_compact(sd->dP);
  ray->dD = differential_zero_compact();
  ray->time = sd->time;

  /* Fill in intersection surface and light details. */
  ray->self.object = (skip_self) ? sd->object : OBJECT_NONE;
  ray->self.prim = (skip_self) ? sd->prim : PRIM_NONE;
  ray->self.light_object = ls->object;
  ray->self.light_prim = ls->prim;
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
    return 0.0f;
  }
  else if (kernel_data.integrator.direct_light_sampling_type == DIRECT_LIGHT_SAMPLING_NEE) {
    return 1.0f;
  }
  else
#endif
    return power_heuristic(nee_pdf, forward_pdf);
}

CCL_NAMESPACE_END
