/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_info.hh"

FRAGMENT_SHADER_CREATE_INFO(gpencil_depth_merge)

void main()
{
  float depth = textureLod(depthBuf, gl_FragCoord.xy / vec2(textureSize(depthBuf, 0)), 0).r;
  if (strokeOrder3d) {
    gl_FragDepth = depth;
  }
  else {
    gl_FragDepth = (depth != 0.0) ? gl_FragCoord.z : 1.0;
  }
}
