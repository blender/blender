/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void rgbtobw(vec4 color, out float outval)
{
  vec3 factors = vec3(0.2126, 0.7152, 0.0722);
  outval = dot(color.rgb, factors);
}
