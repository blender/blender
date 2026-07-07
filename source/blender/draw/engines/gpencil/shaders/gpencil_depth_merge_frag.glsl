/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpencil_depth_merge)

void main()
{
  float depth = textureLod(depth_buf, gl_FragCoord.xy / float2(textureSize(depth_buf, 0)), 0).r;
  if (stroke_order3d) {
    gl_FragDepth = depth;
  }
  else {
    gl_FragDepth = (depth != 0.0f) ? gl_FragCoord.z : 1.0f;
  }
}
