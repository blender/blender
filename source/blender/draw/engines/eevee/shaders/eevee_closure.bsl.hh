/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_bxdf.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"

LightProbeRay bxdf_lightprobe_ray(
    const UtilityTexture &util_tx, BsdfClosure cl, float3 /*P*/, float3 V, Thickness thickness)
{
  cl.context_amend(V, thickness);
  return cl.lightprobe_ray(util_tx, V, thickness);
}

LightEvalContext light_eval_context_create(const UtilityTexture &util_tx, BsdfClosure cl, float3 V)
{
  if (cl.type == CLOSURE_NONE_ID) {
    /* Avoid UB and allow shadow pass on emissive surfaces. */
    cl.type = CLOSURE_BSDF_DIFFUSE_ID;
  }
  if (closure_has_transmission(cl.type)) {
    /* Transmission is evaluated in a separate pass. Use a diffuse context with the unflipped
     * normal, as the shadows of all reflection closures use the normal of the first one. */
    cl.type = CLOSURE_BSDF_DIFFUSE_ID;
  }
  LightEvalContext cl_light = cl.light_eval_context(util_tx, V, Thickness::zero());
  cl_light.light_shadowed = float3(0.0f);
  cl_light.light_unshadowed = float3(0.0f);
  return cl_light;
}

LightEvalContext light_eval_context_create_transmission(const UtilityTexture &util_tx,
                                                        BsdfClosure cl,
                                                        float3 V,
                                                        Thickness thickness)
{
  if (cl.type == CLOSURE_NONE_ID) {
    /* Avoid UB and allow shadow pass on emissive surfaces. */
    cl.type = CLOSURE_BSDF_DIFFUSE_ID;
  }
  LightEvalContext cl_light = cl.light_eval_context(util_tx, V, thickness);
  if (cl.type == CLOSURE_BSSRDF_BURLEY_ID) {
    /* If the `thickness / sss_radius` ratio is near 0, this transmission term should converge
     * to a uniform term like the translucent BSDF. But we need to find what to do in other
     * cases. For now, approximate the transmission term as just back-facing. */
    BsdfClosure translucent = {
        .color = cl.color,
        .N = cl.N,
        .type = CLOSURE_BSDF_TRANSLUCENT_ID,
        .data = float4(0),
    };
    cl_light = translucent.light_eval_context(util_tx, V, Thickness::zero());
  }
  cl_light.light_shadowed = float3(0.0f);
  cl_light.light_unshadowed = float3(0.0f);
  return cl_light;
}
