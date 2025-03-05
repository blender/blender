/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_grid_info.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_grid_background)

void main()
{
  fragColor = ucolor;
  float scene_depth = texelFetch(depthBuffer, ivec2(gl_FragCoord.xy), 0).r;
  fragColor.a = (scene_depth == 1.0) ? 1.0 : 0.0;
}
