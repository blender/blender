/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_safe_lib.glsl"

void node_subsurface_scattering(float4 color,
                                float scale,
                                float3 radius,
                                float ior,
                                float roughness,
                                float anisotropy,
                                float3 N,
                                float weight,
                                out Closure result)
{
  color = max(color, float4(0.0f));
  ior = max(ior, 1e-5f);
  /* roughness = saturate(roughness) */
  N = safe_normalize(N);

  ClosureSubsurface sss_data;
  sss_data.weight = weight;
  sss_data.color = color.rgb;
  sss_data.N = N;
  sss_data.sss_radius = max(radius * scale, float3(0.0f));

  result = closure_eval(sss_data);
}
