/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_subsurface_scattering(vec4 color,
                                float scale,
                                vec3 radius,
                                float ior,
                                float anisotropy,
                                vec3 N,
                                float weight,
                                const float do_sss,
                                out Closure result)
{
  color = max(color, vec4(0.0));
  scale = max(scale, 0.0);
  radius = max(radius, vec3(0));
  ior = max(ior, 1e-5);
  N = safe_normalize(N);

  ClosureSubsurface sss_data;
  sss_data.weight = weight;
  sss_data.color = color.rgb;
  sss_data.N = N;
  sss_data.sss_radius = radius * scale;

#ifdef GPU_SHADER_EEVEE_LEGACY_DEFINES
  if (do_sss == 0.0) {
    /* Flag as disabled. */
    sss_data.sss_radius.b = -1.0;
  }
#endif
  result = closure_eval(sss_data);
}
