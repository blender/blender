/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_grid_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_grid_background)

void main()
{
  frag_color = ucolor;
  float scene_depth = texelFetch(depth_buffer, int2(gl_FragCoord.xy), 0).r;
  frag_color.a = (scene_depth == 1.0f) ? 1.0f : 0.0f;
}
