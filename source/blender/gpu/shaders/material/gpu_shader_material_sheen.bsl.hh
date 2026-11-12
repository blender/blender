/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"
#include "gpu_shader_math_vector_safe.bsl.hh"

[[node]]
void node_bsdf_sheen(float4 color,
                     float roughness,
                     const float3 N,
                     const float weight,
                     KernelGlobals &kg,
                     ShadingData &sd,
                     Closure &result)
{
  const float3 V = coordinate_impl(kg, sd, sd.P, sd.N).incoming;
  color = max(color, float4(0.0f));

  ClosureSheen sheen_data;
  sheen_data.N = safe_normalize(N);
  sheen_data.roughness = saturate(roughness);
  sheen_data.color = weight * color.rgb *
                     sheen_albedo(kg, dot(sheen_data.N, V), sheen_data.roughness);

  result = closure_eval(kg, sd, sheen_data);
}
