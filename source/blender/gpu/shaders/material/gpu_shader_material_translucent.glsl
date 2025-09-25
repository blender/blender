/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_vector_safe_lib.glsl"

void node_bsdf_translucent(float4 color, float3 N, float weight, out Closure result)
{
  color = max(color, float4(0.0f));
  N = safe_normalize(N);

  ClosureTranslucent translucent_data;
  translucent_data.weight = weight;
  translucent_data.color = color.rgb;
  translucent_data.N = N;

  result = closure_eval(translucent_data);
}
