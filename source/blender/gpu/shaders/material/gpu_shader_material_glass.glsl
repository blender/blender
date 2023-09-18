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

  vec2 bsdf = bsdf_lut(NV, roughness, ior, do_multiscatter);

  ClosureReflection reflection_data;
  reflection_data.weight = bsdf.x * weight;
  reflection_data.color = color.rgb;
  reflection_data.N = N;
  reflection_data.roughness = roughness;

  ClosureRefraction refraction_data;
  refraction_data.weight = bsdf.y * weight;
  refraction_data.color = color.rgb;
  refraction_data.N = N;
  refraction_data.roughness = roughness;
  refraction_data.ior = ior;

  result = closure_eval(reflection_data, refraction_data);
}
