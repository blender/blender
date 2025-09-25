/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_area_borders_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_area_borders)

void main()
{
  /* Should be 1.0f but minimize the AA on the edges. */
  float dist = (length(uv) - (0.98f - width)) * scale;

  fragColor = color;
  fragColor.a *= smoothstep(-0.09f, 1.09f, dist);
}
