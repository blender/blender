/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void rgbtobw(float4 color, float3 luminance_coefficients, float &outval)
{
  outval = dot(color.rgb, luminance_coefficients);
}
