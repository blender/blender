/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_bsdf_toon(
    vec4 color, float size, float tsmooth, vec3 N, float weight, out Closure result)
{
  color = max(color, vec4(0.0));
  N = safe_normalize(N);

  /* Fallback to diffuse. */
  ClosureDiffuse diffuse_data;
  diffuse_data.weight = weight;
  diffuse_data.color = color.rgb;
  diffuse_data.N = N;

  result = closure_eval(diffuse_data);
}
