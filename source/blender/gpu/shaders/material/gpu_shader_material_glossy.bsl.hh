/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"
#include "gpu_shader_math_vector_safe.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

[[node]]
void node_bsdf_glossy(float4 color,
                      float roughness,
                      float /*anisotropy*/, /* Unsupported. */
                      float /*rotation*/,   /* Unsupported. */
                      float3 N,
                      float3 /*T*/, /* Unsupported. */
                      float weight,
                      const float do_multiscatter,
                      [[resource_table]] KernelGlobals &kg,
                      ShadingData &sd,
                      Closure &result)
{
  color = max(color, float4(0.0f));
  roughness = saturate(roughness);
  N = safe_normalize(N);
  /* anisotropy = clamp(anisotropy, -0.99f, 0.99f) */

  float3 V = coordinate_impl(kg, sd, sd.P, sd.N).incoming;
  float NV = dot(N, V);

  float3 brdf = brdf_lut(kg, color.rgb, color.rgb, NV, roughness, do_multiscatter != 0.0f);

  ClosureReflection reflection_data;
  reflection_data.color = weight * brdf;
  reflection_data.N = N;
  reflection_data.roughness = roughness;

  result = closure_eval(sd, reflection_data);
}
