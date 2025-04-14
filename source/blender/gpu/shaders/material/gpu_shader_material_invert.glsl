/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void invert(float fac, float4 col, out float4 outcol)
{
  outcol.xyz = mix(col.xyz, float3(1.0f) - col.xyz, fac);
  outcol.w = col.w;
}
