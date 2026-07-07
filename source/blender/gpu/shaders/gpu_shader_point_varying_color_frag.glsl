/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_point_varying_size_varying_color_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_point_varying_size_varying_color)

void main()
{
  float2 centered = gl_PointCoord - float2(0.5f);
  float dist_squared = dot(centered, centered);
  constexpr float rad_squared = 0.25f;

  /* Round point with jagged edges. */
  if (dist_squared > rad_squared) {
    gpu_discard_fragment();
  }

  fragColor = finalColor;
}
