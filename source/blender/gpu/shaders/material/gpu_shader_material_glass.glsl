/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_bsdf_glass(vec4 color,
                     float roughness,
                     float ior,
                     vec3 N,
                     float weight,
                     float do_multiscatter,
                     out Closure result)
{
  N = safe_normalize(N);
  vec3 V = cameraVec(g_data.P);
  float NV = dot(N, V);

  vec2 split_sum = btdf_lut(NV, roughness, ior);

  float fresnel = (do_multiscatter != 0.0) ? split_sum.y : F_eta(ior, NV);
  float btdf = (do_multiscatter != 0.0) ? 1.0 : split_sum.x;

  ClosureReflection reflection_data;
  reflection_data.weight = fresnel * weight;
  reflection_data.color = color.rgb;
  reflection_data.N = N;
  reflection_data.roughness = roughness;

  ClosureRefraction refraction_data;
  refraction_data.weight = (1.0 - fresnel) * weight;
  refraction_data.color = color.rgb * btdf;
  refraction_data.N = N;
  refraction_data.roughness = roughness;
  refraction_data.ior = ior;

  result = closure_eval(reflection_data, refraction_data);
}
