/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_safe_lib.glsl"

void node_bsdf_toon(
    float4 color, float size, float tsmooth, float3 N, float weight, out Closure result)
{
  color = max(color, float4(0.0f));
  N = safe_normalize(N);

  /* Fall back to diffuse. */
  ClosureDiffuse diffuse_data;
  diffuse_data.weight = weight;
  diffuse_data.color = color.rgb;
  diffuse_data.N = N;

  result = closure_eval(diffuse_data);
}
