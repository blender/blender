/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_bsdf_translucent(vec4 color, vec3 N, float weight, out Closure result)
{
  N = safe_normalize(N);

  ClosureTranslucent translucent_data;
  translucent_data.weight = weight;
  translucent_data.color = color.rgb;
  translucent_data.N = -N;

  result = closure_eval(translucent_data);
}
