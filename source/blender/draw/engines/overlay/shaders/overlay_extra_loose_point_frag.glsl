/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_extra_loose_point_base)

void main()
{
  float2 centered = abs(gl_PointCoord - float2(0.5f));
  float dist = max(centered.x, centered.y);

  float fac = dist * dist * 4.0f;
  /* Non linear blend. */
  float4 col1 = sqrt(theme.colors.edit_mesh_middle);
  float4 col2 = sqrt(final_color);
  frag_color = mix(col1, col2, 0.45f + fac * 0.65f);
  frag_color *= frag_color;

  line_output = float4(0.0f);

  /* Make the effect more like a fresnel by offsetting
   * the depth and creating mini-spheres.
   * Disabled as it has performance impact. */
  // gl_FragDepth = gl_FragCoord.z + 1e-6f * fac;
}
