/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void invert(float fac, vec4 col, out vec4 outcol)
{
  outcol.xyz = mix(col.xyz, vec3(1.0) - col.xyz, fac);
  outcol.w = col.w;
}
