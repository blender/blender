/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_bsdf_refraction(
    vec4 color, float roughness, float ior, vec3 N, float weight, out Closure result)
{
  color = max(color, vec4(0));
  roughness = saturate(roughness);
  ior = max(ior, 1e-5);
  N = safe_normalize(N);

  ClosureRefraction refraction_data;
  refraction_data.weight = weight;
  refraction_data.color = color.rgb;
  refraction_data.N = N;
  refraction_data.roughness = roughness;
  refraction_data.ior = ior;

  result = closure_eval(refraction_data);
}
