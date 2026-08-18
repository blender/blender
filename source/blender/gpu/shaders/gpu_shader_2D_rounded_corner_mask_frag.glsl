/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_rounded_corner_mask_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_rounded_corner_mask)

void main()
{
  /* Distance to the arc, negative inside of the rounded rectangle. */
  float dist = length(arc_co) - arc_radius;

  fragColor = color;
  /* A one pixel wide linear ramp centered on the arc. */
  fragColor.a *= clamp(dist + 0.5f, 0.0f, 1.0f);
}
