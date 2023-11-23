/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  vec2 uv = gl_FragCoord.xy / vec2(textureSize(planar_radiance_tx, 0).xy);
  out_color = texture(planar_radiance_tx, vec3(uv, probe_index));
  out_color.a = 0.0;
}
