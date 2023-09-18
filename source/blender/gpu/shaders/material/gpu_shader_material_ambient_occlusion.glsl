/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_ambient_occlusion(vec4 color,
                            float dist,
                            vec3 normal,
                            const float inverted,
                            const float sample_count,
                            out vec4 result_color,
                            out float result_ao)
{
  result_ao = ambient_occlusion_eval(normal, dist, inverted, sample_count);
  result_color = result_ao * color;
}
