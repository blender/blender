/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void rgbtobw(vec4 color, vec3 luminance_coefficients, out float outval)
{
  outval = dot(color.rgb, luminance_coefficients);
}
