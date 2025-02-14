/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_subsurface_scattering(vec4 color,
                                float scale,
                                vec3 radius,
                                float ior,
                                float roughness,
                                float anisotropy,
                                vec3 N,
                                float weight,
                                out Closure result)
{
  color = max(color, vec4(0.0));
  ior = max(ior, 1e-5);
  /* roughness = saturate(roughness) */
  N = safe_normalize(N);

  ClosureSubsurface sss_data;
  sss_data.weight = weight;
  sss_data.color = color.rgb;
  sss_data.N = N;
  sss_data.sss_radius = max(radius * scale, vec3(0.0));

  result = closure_eval(sss_data);
}
