/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_bxdf_diffuse_lib.glsl"
#include "eevee_bxdf_microfacet_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"

/* Return the apparent roughness of a closure compared to a GGX reflection lobe. */
float closure_apparent_roughness_get(ClosureUndetermined cl)
{
  switch (cl.type) {
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
      return bxdf_diffuse_perceived_roughness();
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return bxdf_translucent_perceived_roughness();
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      return bxdf_ggx_perceived_roughness_reflection(to_closure_reflection(cl).roughness);
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      return bxdf_ggx_perceived_roughness_transmission(to_closure_refraction(cl).roughness,
                                                       to_closure_refraction(cl).ior);
    case CLOSURE_NONE_ID:
      return 0.0f;
  }
  return 0.0f;
}

float closure_evaluate_pdf(ClosureUndetermined cl, float3 L, float3 V, float thickness)
{
  switch (cl.type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return bxdf_translucent_eval(cl.N, L, thickness).pdf;
    case CLOSURE_BSSRDF_BURLEY_ID:
      /* TODO(fclem): Sampled BSSDF. */
      return bxdf_diffuse_eval(cl.N, L).pdf;
    case CLOSURE_BSDF_DIFFUSE_ID:
      return bxdf_diffuse_eval(cl.N, L).pdf;
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID: {
      ClosureReflection cl_ = to_closure_reflection(cl);
      float roughness_sq = square(cl_.roughness);
      return bxdf_ggx_eval_reflection(cl.N, L, V, roughness_sq, true).pdf;
    }
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID: {
      ClosureRefraction cl_ = to_closure_refraction(cl);
      float roughness_sq = square(cl_.roughness);
      return bxdf_ggx_eval_refraction(cl.N, L, V, roughness_sq, cl_.ior, thickness, true).pdf;
    }
    case CLOSURE_NONE_ID:
      break;
  }
  assert(false);
  return 0.0f;
}

LightProbeRay bxdf_lightprobe_ray(ClosureUndetermined cl, float3 P, float3 V, float thickness)
{
  switch (cl.type) {
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      bxdf_ggx_context_amend_transmission(cl, V, thickness);
      break;
    case CLOSURE_BSDF_TRANSLUCENT_ID:
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      break;
    case CLOSURE_NONE_ID:
      assert(false);
      break;
  }

  switch (cl.type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return bxdf_translucent_lightprobe(cl.N, thickness);
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
      return bxdf_diffuse_lightprobe(cl.N);
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      return bxdf_ggx_lightprobe_reflection(to_closure_reflection(cl), V);
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      return bxdf_ggx_lightprobe_transmission(to_closure_refraction(cl), V, thickness);
    case CLOSURE_NONE_ID:
      assert(false);
      break;
  }

  LightProbeRay ray;
  return ray;
}

ClosureLight closure_light_new_ex(ClosureUndetermined cl,
                                  float3 V,
                                  float thickness,
                                  const bool is_transmission)
{
  ClosureLight cl_light;
  if (is_transmission) {
    /* Transmission. */
    switch (cl.type) {
      case CLOSURE_BSSRDF_BURLEY_ID:
        /* If the `thickness / sss_radius` ratio is near 0, this transmission term should converge
         * to a uniform term like the translucent BSDF. But we need to find what to do in other
         * cases. For now, approximate the transmission term as just back-facing. */
        cl_light = bxdf_translucent_light(cl, V, 0.0f);
        break;
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
        cl_light = bxdf_ggx_light_transmission(to_closure_refraction(cl), V, thickness);
        break;
      case CLOSURE_BSDF_TRANSLUCENT_ID:
      /* Defaults to avoid UB. */
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      case CLOSURE_BSDF_DIFFUSE_ID:
      case CLOSURE_NONE_ID:
        cl_light = bxdf_translucent_light(cl, V, thickness);
        break;
    }
  }
  else {
    /* Reflection. */
    switch (cl.type) {
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
        cl_light = bxdf_ggx_light_reflection(to_closure_reflection(cl), V);
        break;
      case CLOSURE_BSSRDF_BURLEY_ID:
      case CLOSURE_BSDF_DIFFUSE_ID:
      /* Defaults to avoid UB. */
      case CLOSURE_BSDF_TRANSLUCENT_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      case CLOSURE_NONE_ID:
        cl_light = bxdf_diffuse_light(cl);
        break;
    }
  }
  cl_light.light_shadowed = float3(0.0f);
  cl_light.light_unshadowed = float3(0.0f);
  return cl_light;
}

ClosureLight closure_light_new(ClosureUndetermined cl, float3 V, float thickness)
{
  return closure_light_new_ex(cl, V, thickness, true);
}

ClosureLight closure_light_new(ClosureUndetermined cl, float3 V)
{
  return closure_light_new_ex(cl, V, 0.0f, false);
}
