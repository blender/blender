/*
 * Copyright 2011-2013 Blender Foundation
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

#include "kernel/kernel_light.h"
#include "kernel/kernel_montecarlo.h"
#include "kernel/kernel_path_state.h"
#include "kernel/kernel_shader.h"

CCL_NAMESPACE_BEGIN

/* Evaluate shader on light. */
ccl_device_noinline_cpu float3
light_sample_shader_eval(INTEGRATOR_STATE_ARGS,
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
        INTEGRATOR_STATE_PASS, emission_sd, NULL, PATH_RAY_EMISSION);

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
ccl_device_inline bool light_sample_terminate(ccl_global const KernelGlobals *ccl_restrict kg,
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

ccl_device_inline float3
shadow_ray_smooth_surface_offset(ccl_global const KernelGlobals *ccl_restrict kg,
                                 ccl_private const ShaderData *ccl_restrict sd,
                                 float3 Ng)
{
  float3 V[3], N[3];
  triangle_vertices_and_normals(kg, sd->prim, V, N);

  const float u = sd->u, v = sd->v;
  const float w = 1 - u - v;
  float3 P = V[0] * u + V[1] * v + V[2] * w; /* Local space */
  float3 n = N[0] * u + N[1] * v + N[2] * w; /* We get away without normalization */

  object_normal_transform(kg, sd, &n); /* Normal x scale, world space */

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

ccl_device_inline float3 shadow_ray_offset(ccl_global const KernelGlobals *ccl_restrict kg,
                                           ccl_private const ShaderData *ccl_restrict sd,
                                           float3 L)
{
  float NL = dot(sd->N, L);
  bool transmit = (NL < 0.0f);
  float3 Ng = (transmit ? -sd->Ng : sd->Ng);
  float3 P = ray_offset(sd->P, Ng);

  if ((sd->type & PRIMITIVE_ALL_TRIANGLE) && (sd->shader & SHADER_SMOOTH_NORMAL)) {
    const float offset_cutoff =
        kernel_tex_fetch(__objects, sd->object).shadow_terminator_geometry_offset;
    /* Do ray offset (heavy stuff) only for close to be terminated triangles:
     * offset_cutoff = 0.1f means that 10-20% of rays will be affected. Also
     * make a smooth transition near the threshold. */
    if (offset_cutoff > 0.0f) {
      float NgL = dot(Ng, L);
      float offset_amount = 0.0f;
      if (NL < offset_cutoff) {
        offset_amount = clamp(2.0f - (NgL + NL) / offset_cutoff, 0.0f, 1.0f);
      }
      else {
        offset_amount = clamp(1.0f - NgL / offset_cutoff, 0.0f, 1.0f);
      }
      if (offset_amount > 0.0f) {
        P += shadow_ray_smooth_surface_offset(kg, sd, Ng) * offset_amount;
      }
    }
  }

  return P;
}

ccl_device_inline void shadow_ray_setup(ccl_private const ShaderData *ccl_restrict sd,
                                        ccl_private const LightSample *ccl_restrict ls,
                                        const float3 P,
                                        ccl_private Ray *ray)
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
      ray->D = ray_offset(ls->P, ls->Ng) - P;
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
}

/* Create shadow ray towards light sample. */
ccl_device_inline void light_sample_to_surface_shadow_ray(
    ccl_global const KernelGlobals *ccl_restrict kg,
    ccl_private const ShaderData *ccl_restrict sd,
    ccl_private const LightSample *ccl_restrict ls,
    ccl_private Ray *ray)
{
  const float3 P = shadow_ray_offset(kg, sd, ls->D);
  shadow_ray_setup(sd, ls, P, ray);
}

/* Create shadow ray towards light sample. */
ccl_device_inline void light_sample_to_volume_shadow_ray(
    ccl_global const KernelGlobals *ccl_restrict kg,
    ccl_private const ShaderData *ccl_restrict sd,
    ccl_private const LightSample *ccl_restrict ls,
    const float3 P,
    ccl_private Ray *ray)
{
  shadow_ray_setup(sd, ls, P, ray);
}

CCL_NAMESPACE_END
