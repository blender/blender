/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_depth_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(workbench_merge_depth)

void main()
{
  float2 screen_uv = gl_FragCoord.xy / float2(textureSize(depth_tx, 0));
  gl_FragDepth = texture(depth_tx, screen_uv).r;
}
